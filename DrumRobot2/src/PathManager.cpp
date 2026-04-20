// PathManager.cpp
#include "../include/managers/PathManager.hpp"
#include "../DynamixelSDK-3.8.4/c++/include/dynamixel_sdk/dynamixel_sdk.h"


PathManager::PathManager(State &stateRef,
                         CanManager &canManagerRef,
                         std::map<std::string, std::shared_ptr<GenericMotor>> &motorsRef,
                         USBIO &usbioRef,
                         Functions &funRef)
    : state(stateRef), canManager(canManagerRef), motors(motorsRef), usbio(usbioRef), func(funRef)
{
}

// Public

void PathManager::initPathManager()
{
    setDrumCoordinate();        // 드럼 악기 위치 읽기
    setWristAngleOnImpact();    // 타격 시 손목각도 설정
    setAddStanceAngle();        // AddStance 에서 사용하는 pos 설정
}

void PathManager::genAndPushAddStance(string flagName)
{
    VectorXd Q1 = VectorXd::Zero(12);
    VectorXd Q2 = VectorXd::Zero(12);

    // Q1 : finalMotorPosition -> 마지막 모터 명령값에서 이어서 생성
    Q1 = getFinalMotorPosition();

    // Q2 : flag 에 따라 정해진 위치
    Q2 = getAddStanceAngle(flagName);

    // 터미널 출력
    for (int k = 0; k < 12; k++)
    {
        std::cout << "Q1[" << k << "] : " << Q1[k] * 180.0 / M_PI << " [deg] -> Q2[" << k << "] : " << Q2[k] * 180.0 / M_PI << " [deg]\n";
    }
    
    // push
    pushAddStance(Q1, Q2);

    // DXL
    pushAddStanceDXL(flagName);
}

void PathManager::initPlayStateValue()
{
    startOfPlay = false;        // true로 변경시키면 연주 시작
    endOfPlayCommand = false;   // 모든 명령 생성 후 true로 변경

    lineOfScore = 0;            // 현재 악보 읽은 줄.

    measureStateR.resize(3);    // [이전 시간, 이전 악기, state]
    measureStateR = VectorXd::Zero(3);
    measureStateR(1) = 1.0;     // 시작 위치 SN

    measureStateL.resize(3);
    measureStateL = VectorXd::Zero(3);
    measureStateL(1) = 1.0;     // 시작 위치 SN

    roundSum = 0.0;             // 5ms 스텝 단위에서 누적되는 오차 보상

    waistParameterQueue = std::queue<WaistParameter>();   // 큐 재선언해서 비우기

    hitStateR = VectorXd::Zero(3);  // [이전 시간, 이전 state, intensity]
    hitStateL = VectorXd::Zero(3); 

    prevLine = VectorXd::Zero(9);   // 악보 나눌 때 시작 악보 기록

    waistAngleT1 = readyAngle(0);   // 허리 시작 위치
    waistAngleT0 = readyAngle(0);   // 허리 이전 위치
    waistTimeT0 = -1;               // 허리 이전 시간

    prevWaistPos = 0.0;         // 브레이크 판단에 사용될 허리 전 값
    preDiff = 0.0;              // 브레이크 판단(필터)에 사용될 전 허리 차이값

    getVelocityRadps(true, 0.0, 0);     // 속도 계산을 위한 초기값
}

void PathManager::processLine(MatrixXd &measureMatrix)
{
    lineOfScore++;
    std::cout << "\n//////////////////////////////// Read Measure : " << lineOfScore << "\n";
    // std::cout << measureMatrix;
    // std::cout << "\n ////////////// \n";

    // ***허리 브레이크 테스트용*** // (이인우)
    // sleep(1);

    if (measureMatrix.rows() > 1)
    {
        // avoidCollision(measureMatrix);  // 충돌 회피
        genTrajectory(measureMatrix);   // 궤적 생성
    }

    if (lineOfScore > preCreatedLine)
    {
        solveIKandPushCommand();        // IK & 명령 생성
    }
}

void PathManager::clearCommandBuffers()
{
    // 모터의 명령 버퍼 비우기
    for (auto &motor_pair : motors)
    {
        if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(motor_pair.second))
        {
            std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
            std::queue<TMotorData>().swap(tMotor->commandBuffer);
        }
        else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(motor_pair.second))
        {
            std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);
            std::queue<MaxonData>().swap(maxonMotor->commandBuffer);
        }
    }

    // PathManager의 내부 큐 비우기
    std::queue<TaskSpaceTrajectory>().swap(taskSpaceQueue);
    std::queue<WaistParameter>().swap(waistParameterQueue);
    std::queue<HitTrajectory>().swap(hitQueue);
    std::queue<PedalTrajectory>().swap(pedalQueue);
    
    std::lock_guard<std::mutex> lock(dxlBufferMutex);
    std::queue<vector<vector<float>>>().swap(dxlCommandBuffer);
}

// Private

////////////////////////////////////////////////////////////////////////////////
/*                               Initialization                               */
////////////////////////////////////////////////////////////////////////////////

void PathManager::setDrumCoordinate()
{
    ifstream inputFile("../include/drum/drum_position.txt");

    if (!inputFile.is_open())
    {
        cerr << "Failed to open the file."
             << "\n";
    }

    // Read data into a 2D vector
    MatrixXd instXYZ(6, 10);

    for (int i = 0; i < 6; ++i)
    {
        for (int j = 0; j < 10; ++j)
        {
            inputFile >> instXYZ(i, j);
            // std::cout << instXYZ(i, j) << " ";
        }
        // std::cout <<"\n";
    }

    // Extract the desired elements
    Vector3d right_S;
    Vector3d right_FT;
    Vector3d right_MT;
    Vector3d right_HT;
    Vector3d right_HH;
    Vector3d right_R;
    Vector3d right_RC;
    Vector3d right_LC;
    Vector3d right_OHH;
    Vector3d right_RB;

    Vector3d left_S;
    Vector3d left_FT;
    Vector3d left_MT;
    Vector3d left_HT;
    Vector3d left_HH;
    Vector3d left_R;
    Vector3d left_RC;
    Vector3d left_LC;
    Vector3d left_OHH;
    Vector3d left_RB;

    for (int i = 0; i < 3; ++i)
    {
        right_S(i) = instXYZ(i, 0);
        right_FT(i) = instXYZ(i, 1);
        right_MT(i) = instXYZ(i, 2);
        right_HT(i) = instXYZ(i, 3);
        right_HH(i) = instXYZ(i, 4);
        right_R(i) = instXYZ(i, 5);
        right_RC(i) = instXYZ(i, 6);
        right_LC(i) = instXYZ(i, 7);
        right_OHH(i) = instXYZ(i, 8);
        right_RB(i) = instXYZ(i, 9);

        left_S(i) = instXYZ(i + 3, 0);
        left_FT(i) = instXYZ(i + 3, 1);
        left_MT(i) = instXYZ(i + 3, 2);
        left_HT(i) = instXYZ(i + 3, 3);
        left_HH(i) = instXYZ(i + 3, 4);
        left_R(i) = instXYZ(i + 3, 5);
        left_RC(i) = instXYZ(i + 3, 6);
        left_LC(i) = instXYZ(i + 3, 7);
        left_OHH(i) = instXYZ(i + 3, 8);
        left_RB(i) = instXYZ(i + 3, 9);
    }

    drumCoordinateR.resize(3, 10);
    drumCoordinateL.resize(3, 10);

    drumCoordinateR << right_S, right_FT, right_MT, right_HT, right_HH, right_R, right_RC, right_LC, right_OHH, right_RB;
    drumCoordinateL << left_S,  left_FT,  left_MT,  left_HT,  left_HH,  left_R,  left_RC,  left_LC,  left_OHH,  left_RB;
}

void PathManager::setWristAngleOnImpact()
{
    // 악기 별 타격 시 손목 각도
    wristAngleOnImpactR.resize(1, 10);
    wristAngleOnImpactL.resize(1, 10);

    //                          S                  FT                  MT                  HT                  HH                  R                   RC                 LC                Open HH           RB
    wristAngleOnImpactR << 10.0*M_PI/180.0,   10.0*M_PI/180.0,     5.0*M_PI/180.0,     5.0*M_PI/180.0,    10.0*M_PI/180.0,    15.0*M_PI/180.0,    10.0*M_PI/180.0,   10.0*M_PI/180.0,  10.0*M_PI/180.0, 10.0*M_PI/180.0;
    wristAngleOnImpactL << 10.0*M_PI/180.0,   10.0*M_PI/180.0,     5.0*M_PI/180.0,     5.0*M_PI/180.0,    10.0*M_PI/180.0,    15.0*M_PI/180.0,    10.0*M_PI/180.0,   10.0*M_PI/180.0,  10.0*M_PI/180.0, 10.0*M_PI/180.0;
}

void PathManager::setAddStanceAngle()
{
    //////////////////////////////////////// Ready Angle
    setReadyAngle();

    //////////////////////////////////////// Home Angle
    homeAngle.resize(14);
    //              waist          R_arm1         L_arm1
    homeAngle << 10*M_PI/180.0,  90*M_PI/180.0,  90*M_PI/180.0,
    //              R_arm2         R_arm3         L_arm2
                0*M_PI/180.0,   90*M_PI/180.0,  0*M_PI/180.0,
    //              L_arm3         R_wrist        L_wrist
                90*M_PI/180.0, 75*M_PI/180.0, 75*M_PI/180.0,
    //          Test               R_foot         L_foot            
                0*M_PI/180.0,   0*M_PI/180.0,   0*M_PI/180.0,
    //          DXL1            DXL2
                0*M_PI/180.0,   90*M_PI/180.0;

    //////////////////////////////////////// Shutdown Angle
    shutdownAngle.resize(14);
        //              waist          R_arm1         L_arm1
    shutdownAngle << 0*M_PI/180.0, 135*M_PI/180.0, 45*M_PI/180.0,
    //                  R_arm2         R_arm3         L_arm2
                    0*M_PI/180.0,  20*M_PI/180.0,   0*M_PI/180.0,
    //                  L_arm3         R_wrist        L_wrist
                    20*M_PI/180.0,  90*M_PI/180.0,  90*M_PI/180.0,
    //          Test               R_foot         L_foot            
                    0,                 0,              0,
    //          DXL1            DXL2
                0*M_PI/180.0,   120*M_PI/180.0;
}

void PathManager::setReadyAngle()
{
    readyAngle.resize(14);

    VectorXd defaultInstrumentR;    /// 오른팔 시작 위치 (ready 위치)
    VectorXd defaultInstrumentL;    /// 왼팔 시작 위치 (ready 위치)

    VectorXd instrumentVector(20);

    defaultInstrumentR.resize(10);
    defaultInstrumentL.resize(10);
    defaultInstrumentR << 1, 0, 0, 0, 0, 0, 0, 0, 0, 0;    // SN
    defaultInstrumentL << 1, 0, 0, 0, 0, 0, 0, 0, 0, 0;    // SN

    instrumentVector << defaultInstrumentR,
        defaultInstrumentL;

    // x,y,z position
    MatrixXd combined(6, 20);
    combined << drumCoordinateR, MatrixXd::Zero(3, 10), MatrixXd::Zero(3, 10), drumCoordinateL;
    MatrixXd p = combined * instrumentVector;

    VectorXd pR = VectorXd::Map(p.data(), 3, 1);
    VectorXd pL = VectorXd::Map(p.data() + 3, 3, 1);

    // 타격 시 손목 각도
    combined.resize(2, 20);
    combined << wristAngleOnImpactR, MatrixXd::Zero(1, 10), MatrixXd::Zero(1, 10), wristAngleOnImpactL;
    MatrixXd defaultWristAngle = combined * instrumentVector;

    // 허리 각도 구하기
    VectorXd waistParams = getWaistParams(pR, pL, defaultWristAngle(0), defaultWristAngle(1));
    
    // IK
    bool printError = false;
    VectorXd sol = solveGeometricIK(pR, pL, 0.5 * (waistParams(0) + waistParams(1)), defaultWristAngle(0), defaultWristAngle(1), printError);

    for (int i = 0; i < 9; i++)
    {
        readyAngle(i) = sol(i);
    }

    // stay angle 더하기
    ElbowAngle eA;
    WristAngle wA;
    BassAngle bA;
    HihatAngle hA;

    readyAngle(4) += eA.stayAngle;
    readyAngle(6) += eA.stayAngle;
    readyAngle(7) += wA.stayAngle;
    readyAngle(8) += wA.stayAngle;

    // foot
    readyAngle(9) = 0;
    readyAngle(10) = bA.stayAngle;
    readyAngle(11) = hA.openAngle;

    // DXL
    readyAngle(12) = 0.0*M_PI/180.0;
    readyAngle(13) = 110.0*M_PI/180.0;

    // ***허리 브레이크 테스트용*** // (이인우)
    // readyAngle(0) = 0.2;
}

////////////////////////////////////////////////////////////////////////////////
/*                                 AddStance                                  */
////////////////////////////////////////////////////////////////////////////////

VectorXd PathManager::getFinalMotorPosition()
{
    VectorXd Qf = VectorXd::Zero(12);

    // finalMotorPosition 가져오기 (마지막 명령값)
    for (auto &entry : motors)
    {
        int can_id = canManager.motorMapping[entry.first];

        if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
        {
            Qf(can_id) = tMotor->motorPositionToJointAngle(tMotor->finalMotorPosition);
        }
        if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
        {
            Qf(can_id) = maxonMotor->motorPositionToJointAngle(maxonMotor->finalMotorPosition);
        }
    }

    return Qf;
}

VectorXd PathManager::getAddStanceAngle(string flagName)
{
    VectorXd Qf = VectorXd::Zero(12);

    if (flagName == "isHome")
    {
        for (int i = 0; i < 12; i++)
        {
            Qf(i) = homeAngle(i);
        }
    }
    else if (flagName == "isReady")
    {
        for (int i = 0; i < 12; i++)
        {
            Qf(i) = readyAngle(i);
        }
    }
    else if (flagName == "isShutDown")
    {
        for (int i = 0; i < 12; i++)
        {
            Qf(i) = shutdownAngle(i);
        }
    }
    else
    {
        std::cout << "AddStance Flag Error !\n";
    }

    return Qf;
}

void PathManager::pushAddStance(VectorXd &Q1, VectorXd &Q2)
{
    VectorXd Qt = VectorXd::Zero(12);
    VectorXd Vt = VectorXd::Zero(12);
    float dt = canManager.DTSECOND;     // 5ms
    double moveTime = 2.0;              // 2초동안 이동
    double waitTime = 1.0;              // 1초 대기 후 이동 시작
    int moveN = (int)(moveTime / dt);   // 이동 명령 개수 (5ms)
    int waitN = (int)(waitTime / dt);   // 대기 명령 개수 (5ms)

    // 사다리꼴 속도 프로파일 생성을 위한 최고속도 계산
    VectorXd Vmax = VectorXd::Zero(12);
    Vmax = calVmax(Q1, Q2, moveTime);

    // 궤적 생성
    for (int k = 1; k <= moveN + waitN; ++k)
    {
        if (k > waitN)  // 이동 궤적 생성
        {
            float t = (k - waitN) * moveTime / moveN;

            Qt = makeProfile(Q1, Q2, Vmax, t, moveTime);
            Vt = makeVelocityProfile(Q1, Q2, Vmax, t, moveTime);

            for (auto &entry : motors)
            {
                int can_id = canManager.motorMapping[entry.first];

                if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
                {
                    TMotorData newData;
                    newData.position = tMotor->jointAngleToMotorPosition(Qt[can_id]);
                    if (tmotorMode == "position")
                    {
                        newData.mode = tMotor->Position;
                        newData.velocityERPM = 0.0;
                    }
                    // else if (tmotorMode == "velocityFF")
                    // {
                    //     newData.mode = tMotor->VelocityFF;
                    //     newData.velocityERPM = tMotor->cwDir * Vt[can_id] * tMotor->pole * tMotor->gearRatio * 60.0 / 2.0 / M_PI;
                    // }
                    // else if (tmotorMode == "velocityFB")
                    // {
                    //     newData.mode = tMotor->VelocityFB;
                    //     newData.velocityERPM = 0.0;
                    // }
                    else if (tmotorMode == "velocity")
                    {
                        newData.mode = tMotor->Velocity;
                        newData.velocityERPM = tMotor->cwDir * Vt[can_id] * tMotor->pole * tMotor->gearRatio * 60.0 / 2.0 / M_PI;
                    }
                    newData.useBrake = 0;
                    {
                        std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
                        tMotor->commandBuffer.push(newData);
                    }

                    tMotor->finalMotorPosition = newData.position;
                }
                else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
                {
                    // Maxom 모터는 1ms 로 동작
                    for (int i = 0; i < 5; i++)
                    {
                        MaxonData newData;
                        newData.position = maxonMotor->jointAngleToMotorPosition(Qt[can_id]);
                        newData.mode = maxonMotor->CSP;
                        newData.kp = 0.0;
                        newData.kd = 0.0;
                        {
                            std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);
                            maxonMotor->commandBuffer.push(newData);
                        }

                        maxonMotor->finalMotorPosition = newData.position;
                    }
                    maxonMotor->pre_q = Qt[can_id];
                }
            }
        }
        else    // 현재 위치 대기
        {
            for (auto &entry : motors)
            {
                int can_id = canManager.motorMapping[entry.first];

                if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
                {
                    TMotorData newData;
                    newData.position = tMotor->jointAngleToMotorPosition(Q1[can_id]);
                    if (tmotorMode == "position")
                    {
                        newData.mode = tMotor->Position;
                        newData.velocityERPM = 0.0;
                    }
                    // else if (tmotorMode == "velocityFF")
                    // {
                    //     newData.mode = tMotor->VelocityFF;
                    //     newData.velocityERPM = 0.0;
                    // }
                    // else if (tmotorMode == "velocityFB")
                    // {
                    //     newData.mode = tMotor->VelocityFB;
                    //     newData.velocityERPM = 0.0;
                    // }
                    else if (tmotorMode == "velocity")
                    {
                        newData.mode = tMotor->Velocity;
                        newData.velocityERPM = 0.0;
                    }
                    newData.useBrake = 0;
                    {
                        std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
                        tMotor->commandBuffer.push(newData);
                    }
                }
                else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
                {
                    // Maxom 모터는 1ms 로 동작
                    for (int i = 0; i < 5; i++)
                    {
                        MaxonData newData;
                        newData.position = maxonMotor->jointAngleToMotorPosition(Q1[can_id]);
                        newData.mode = maxonMotor->CSP;
                        newData.kp = 0.0;
                        newData.kd = 0.0;
                        {
                            std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);
                            maxonMotor->commandBuffer.push(newData);
                        }
                    }
                    maxonMotor->pre_q = Qt[can_id];
                }
            }
        }
    }
}

VectorXd PathManager::calVmax(VectorXd &q1, VectorXd &q2, float t2)
{
    VectorXd Vmax = VectorXd::Zero(12);

    for (int i = 0; i < 12; i++)
    {
        double val;
        double S = abs(q2(i) - q1(i)); // overflow방지

        if (S > t2 * t2 * accMax / 4)
        {
            // 주어진 가속도로 도달 불가능
            // -1 반환
            val = -1;
        }
        else
        {
            // 2차 방정식 계수
            double A = 1 / accMax;
            double B = -1 * t2;
            double C = S;

            // 2차 방정식 해
            double sol1 = (-B + sqrt(B * B - 4 * A * C)) / 2 / A;
            double sol2 = (-B - sqrt(B * B - 4 * A * C)) / 2 / A;

            if (sol1 >= 0 && sol1 <= accMax * t2 / 2)
            {
                val = sol1;
            }
            else if (sol2 >= 0 && sol2 <= accMax * t2 / 2)
            {
                val = sol2;
            }
            else
            {
                // 해가 범위 안에 없음
                // -2 반환
                val = -2;
            }
        }

        Vmax(i) = val;
    }

    return Vmax;
}

VectorXd PathManager::makeProfile(VectorXd &q1, VectorXd &q2, VectorXd &Vmax, float t, float t2)
{
    VectorXd Qi = VectorXd::Zero(12);

    for (int i = 0; i < 12; i++)
    {
        double val, S;
        int sign;

        S = q2(i) - q1(i);

        // 부호 확인, 이동거리 양수로 변경
        if (S < 0)
        {
            S = abs(S);
            sign = -1;
        }
        else
        {
            sign = 1;
        }

        // 궤적 생성
        if (S == 0)
        {
            // 정지
            val = q1(i);
        }
        else if (Vmax(i) < 0)
        {
            // Vmax 값을 구하지 못했을 때 삼각형 프로파일 생성
            double acc_tri = 4 * S / t2 / t2;

            if (t < t2 / 2)
            {
                val = q1(i) + sign * 0.5 * acc_tri * t * t;
            }
            else if (t < t2)
            {
                val = q2(i) - sign * 0.5 * acc_tri * (t2 - t) * (t2 - t);
            }
            else
            {
                val = q2(i);
            }
        }
        else
        {
            // 사다리꼴 프로파일
            if (t < Vmax(i) / accMax)
            {
                // 가속
                val = q1(i) + sign * 0.5 * accMax * t * t;
            }
            else if (t < S / Vmax(i))
            {
                // 등속
                val = q1(i) + (sign * 0.5 * Vmax(i) * Vmax(i) / accMax) + (sign * Vmax(i) * (t - Vmax(i) / accMax));
            }
            else if (t < Vmax(i) / accMax + S / Vmax(i))
            {
                // 감속
                val = q2(i) - sign * 0.5 * accMax * (S / Vmax(i) + Vmax(i) / accMax - t) * (S / Vmax(i) + Vmax(i) / accMax - t);
            }
            else
            {
                val = q2(i);
            }
        }

        Qi(i) = val;
    }

    return Qi;
}

VectorXd PathManager::makeVelocityProfile(VectorXd &q1, VectorXd &q2, VectorXd &Vmax, float t, float t2)
{
    VectorXd Vi = VectorXd::Zero(12);

    for (int i = 0; i < 12; i++)
    {
        double val, S;
        int sign;

        S = q2(i) - q1(i);

        // 부호 확인, 이동거리 양수로 변경
        if (S < 0)
        {
            S = abs(S);
            sign = -1;
        }
        else
        {
            sign = 1;
        }

        // 궤적 생성
        if (S == 0)
        {
            // 정지
            val = 0.0;
        }
        else if (Vmax(i) < 0)
        {
            // Vmax 값을 구하지 못했을 때 삼각형 프로파일 생성
            double acc_tri = 4 * S / t2 / t2;

            if (t < t2 / 2)
            {
                val = sign * acc_tri * t;
            }
            else if (t < t2)
            {
                val = sign * acc_tri * (t2 - t);
            }
            else
            {
                val = 0.0;
            }
        }
        else
        {
            // 사다리꼴 프로파일
            if (t < Vmax(i) / accMax)
            {
                // 가속
                val = sign * accMax * t;
            }
            else if (t < S / Vmax(i))
            {
                // 등속
                val = sign * Vmax(i);
            }
            else if (t < Vmax(i) / accMax + S / Vmax(i))
            {
                // 감속
                val = sign * accMax * (S / Vmax(i) + Vmax(i) / accMax - t);
            }
            else
            {
                val = 0.0;
            }
        }

        Vi(i) = val;
    }

    return Vi;
}

void PathManager::pushAddStanceDXL(string flagName)
{
    float totalTime = 3.0;  // moveTime + waitTime
    float dxl1 = 0.0, dxl2 = 0.0; // [rad]

    if (flagName == "isHome")
    {
        dxl1 = homeAngle(12);
        dxl2 = homeAngle(13);
    }
    else if (flagName == "isReady")
    {
        dxl1 = readyAngle(12);
        dxl2 = readyAngle(13);
    }
    else if (flagName == "isShutDown")
    {
        dxl1 = shutdownAngle(12);
        dxl2 = shutdownAngle(13);
    }

    vector<vector<float>> dxlCommand = {{totalTime/2, totalTime, dxl1}, {totalTime/2, totalTime, dxl2}};
    {
        std::lock_guard<std::mutex> lock(dxlBufferMutex);
        dxlCommandBuffer.push(dxlCommand);
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                                    Play                                    */
////////////////////////////////////////////////////////////////////////////////

void PathManager::avoidCollision(MatrixXd &measureMatrix)
{
    if (detectCollision(measureMatrix))    // 충돌 예측
    {
        for (int priority = 0; priority < 5; priority++)    // 수정방법 중 우선순위 높은 것부터 시도
        {
            if (modifyMeasure(measureMatrix, priority))     // 주어진 방법으로 회피되면 measureMatrix를 바꾸고 True 반환
            {
                std::cout << measureMatrix;
                std::cout << "\n 충돌 회피 성공 \n";
                break;
            }
        }
    }
    else
    {
        // std::cout << "\n 충돌 안함 \n";
    }
}

void PathManager::genTrajectory(MatrixXd &measureMatrix)
{
    int n = getNumCommands(measureMatrix);      // 명령 개수
    
    genTaskSpaceTrajectory(measureMatrix, n);   // task space 궤적 생성
    genHitTrajectory(measureMatrix, n);         // 타격 궤적 생성
    genPedalTrajectory(measureMatrix, n);       // 발모터 궤적 생성
    genDxlTrajectory(measureMatrix, n);         // DXL모터 궤적 생성

    ///////////////////////////////////////////////////////////// 읽은 줄 삭제
    MatrixXd tmpMatrix(measureMatrix.rows() - 1, measureMatrix.cols());
    tmpMatrix = measureMatrix.block(1, 0, tmpMatrix.rows(), tmpMatrix.cols());
    measureMatrix.resize(tmpMatrix.rows(), tmpMatrix.cols());
    measureMatrix = tmpMatrix;
}

void PathManager::solveIKandPushCommand()
{
    std::vector<WaistParameter> WP = waistParamsQueueToVector();    // waistParameterQueue를 벡터로 변환
    waistParameterQueue.pop();

    int n = WP[0].n;   // 명령 개수

    MatrixXd waistCoefficient = makeWaistCoefficient(WP);   // 허리 궤적 생성

    // 여기서 첫 접근 때 정지하기
    while(!startOfPlay) // 시작 신호 받을 때까지 대기
    {
        usleep(500);
    }

    for (int i = 0; i < n; i++)
    {
        double q0 = getWaistAngle(waistCoefficient, i); // 허리 관절각

        // ***허리 브레이크 테스트용*** // (이인우)
        // q0 = 0.2;

        double KpRatioR, KpRatioL;
        VectorXd q = getJointAngles(q0, KpRatioR, KpRatioL);                // 로봇 관절각
        
        // // 데이터 기록
        // for (int i = 0; i < 9; i++)
        // {
        //     std::string fileName = "solveIK_q" + to_string(i);
        //     func.appendToCSV(fileName, false, i, q(i));
        // }

        // //* 테스트용 *// 모터 연결하면 지워야 함 (이인우)
        // for (int i = 0; i < 7; i++)
        // {
        //     std::string fileName = "solveIK_v" + to_string(i);
        //     func.appendToCSV(fileName, false, i, getVelocityRadps(false, q[i], i));
        // }

        pushCommandBuffer(q, KpRatioR, KpRatioL);                           // 명령 생성 후 push
        pushDxlBuffer(q0);
    }

    if (waistParameterQueue.empty())    // DrumRobot 에게 끝났음 알리기
    {
        endOfPlayCommand = true;    // 모든 명령 생성 완료
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                       Play : Trajectory (Task Space)                       */
////////////////////////////////////////////////////////////////////////////////

int PathManager::getNumCommands(MatrixXd &measureMatrix)
{
    double n;
    double dt = canManager.DTSECOND;

    double t1 = measureMatrix(0, 8);
    double t2 = measureMatrix(1, 8);

    // 한 라인의 데이터 개수 (5ms 단위)
    n = (t2 - t1) / dt;
    roundSum += (int)(n * 10000) % 10000;
    if (roundSum >= 10000)
    {
        roundSum -= 10000;
        n++;
    }
    n = floor(n);

    return (int)n;
}

void PathManager::genTaskSpaceTrajectory(MatrixXd &measureMatrix, int n)
{
    double sR, sL;
    double dt = canManager.DTSECOND;

    // 출발 시간/위치, 도착 시간/위치
    TrajectoryData data = getTrajectoryData(measureMatrix, measureStateR, measureStateL);
    // state update
    measureStateR = data.nextStateR;
    measureStateL = data.nextStateL;

    for (int i = 0; i < n; i++)
    {
        TaskSpaceTrajectory TT;

        // 시간 변환
        double tR = dt * i + data.t1 - data.initialTimeR;
        double tL = dt * i + data.t1 - data.initialTimeL;

        // time scaling (s : 0 -> 1)
        sR = calTimeScaling(0.0, data.finalTimeR - data.initialTimeR, tR);
        sL = calTimeScaling(0.0, data.finalTimeL - data.initialTimeL, tL);
        
        // task space 경로
        TT.trajectoryR = makeTaskSpacePath(data.initialPositionR, data.finalPositionR, sR);
        TT.trajectoryL = makeTaskSpacePath(data.initialPositionL, data.finalPositionL, sL);

        // IK 풀기 위한 손목 각도
        TT.wristAngleR = sR*(data.finalWristAngleR - data.initialWristAngleR) + data.initialWristAngleR;
        TT.wristAngleL = sL*(data.finalWristAngleL - data.initialWristAngleL) + data.initialWristAngleL;

        taskSpaceQueue.push(TT);

        // // 데이터 저장
        // std::string fileName;
        // fileName = "Trajectory_R";
        // func.appendToCSV(fileName, false, TT.trajectoryR[0], TT.trajectoryR[1], TT.trajectoryR[2]);
        // fileName = "Trajectory_L";
        // func.appendToCSV(fileName, false, TT.trajectoryL[0], TT.trajectoryL[1], TT.trajectoryL[2]);
        // fileName = "S";
        // func.appendToCSV(fileName, false, sR, sL);

        if (i == 0)
        {
            // 명령 개수, 허리 범위, 최적화 각도 계산 및 저장
            VectorXd waistParams = getWaistParams(TT.trajectoryR, TT.trajectoryL, TT.wristAngleR, TT.wristAngleL);
            storeWaistParams(n, waistParams);
        }
    }
}

PathManager::TrajectoryData PathManager::getTrajectoryData(MatrixXd &measureMatrix, VectorXd &stateR, VectorXd &stateL)
{
    TrajectoryData data;

    VectorXd measureTime = measureMatrix.col(8);
    VectorXd measureInstrumentR = measureMatrix.col(2);
    VectorXd measureInstrumentL = measureMatrix.col(3);
    VectorXd measureHihat = measureMatrix.col(7);

    data.t1 = measureMatrix(0, 8);
    data.t2 = measureMatrix(1, 8);

    // std::cout << "\n /// t1 -> t2 : " << data.t1 << " -> " << data.t2 << " : " << data.t2 - data.t1 <<  "\n";

    // parse
    pair<VectorXd, VectorXd> dataR = parseTrajectoryData(measureTime, measureInstrumentR, measureHihat, stateR);
    pair<VectorXd, VectorXd> dataL = parseTrajectoryData(measureTime, measureInstrumentL, measureHihat, stateL);

    // state 업데이트
    data.nextStateR = dataR.second;
    data.nextStateL = dataL.second;

    // 시간
    data.initialTimeR = dataR.first(0);
    data.initialTimeL = dataL.first(0);

    data.finalTimeR = dataR.first(11);
    data.finalTimeL = dataL.first(11);

    // 악기
    VectorXd initialInstrumentR = dataR.first.block(1, 0, 10, 1);
    VectorXd initialInstrumentL = dataL.first.block(1, 0, 10, 1);

    VectorXd finalInstrumentR = dataR.first.block(12, 0, 10, 1);
    VectorXd finalInstrumentL = dataL.first.block(12, 0, 10, 1);

    pair<VectorXd, double> initialTagetR = getTargetPosition(initialInstrumentR, 'R');
    pair<VectorXd, double> initialTagetL = getTargetPosition(initialInstrumentL, 'L');

    pair<VectorXd, double> finalTagetR = getTargetPosition(finalInstrumentR, 'R');
    pair<VectorXd, double> finalTagetL = getTargetPosition(finalInstrumentL, 'L');

    // position
    data.initialPositionR = initialTagetR.first;
    data.initialPositionL = initialTagetL.first;

    data.finalPositionR = finalTagetR.first;
    data.finalPositionL = finalTagetL.first;

    // 타격 시 손목 각도
    data.initialWristAngleR = initialTagetR.second;
    data.initialWristAngleL = initialTagetL.second;
    
    data.finalWristAngleR = finalTagetR.second;
    data.finalWristAngleL = finalTagetL.second;

    return data;
}

pair<VectorXd, VectorXd> PathManager::parseTrajectoryData(VectorXd &t, VectorXd &inst, VectorXd &hihat, VectorXd &stateVector)
{
    map<int, int> instrumentMapping = {
        {1, 0}, {2, 1}, {3, 2}, {4, 3}, {5, 4}, {6, 5}, {7, 6}, {8, 7}, {11, 0}, {51, 0}, {61, 0}, {71, 0}, {81, 0}, {91, 0}, {9, 8}, {10, 9}};
    //    S       FT      MT      HT      HH       R      RC      LC       S        S        S        S        S        S     Open HH   RB

    VectorXd initialInstrument = VectorXd::Zero(10), finalInstrument = VectorXd::Zero(10);
    VectorXd outputVector = VectorXd::Zero(22);

    VectorXd nextStateVector;

    bool detectHit = false;
    double detectTime = 0, initialT, finalT;
    int detectInst = 0, initialInstNum, finalInstNum;
    int preState, nextState;
    const double e = 0.00001; 
    double hitDetectionThreshold = 1.2 * 100.0 / bpmOfScore;

    // 타격 감지
    for (int i = 1; i < t.rows(); i++)
    {
        if (round(10000 * (hitDetectionThreshold + e)) < round(10000 * (t(i) - t(0))))
        {
            break;
        }

        if (inst(i) != 0)
        {
            detectHit = true;
            detectTime = t(i);
            detectInst = checkOpenHihat(inst(i), hihat(i));

            break;
        }
    }

    // inst
    preState = stateVector(2);

    // 타격으로 끝나지 않음
    if (inst(0) == 0)
    {
        // 궤적 생성 중
        if (preState == 2 || preState == 3)
        {
            nextState = preState;

            initialInstNum = stateVector(1);
            finalInstNum = detectInst;

            initialT = stateVector(0);
            finalT = detectTime;
        }
        else
        {
            // 다음 타격 감지
            if (detectHit)
            {
                nextState = 2;

                initialInstNum = stateVector(1);
                finalInstNum = detectInst;

                initialT = t(0);
                finalT = detectTime;
            }
            // 다음 타격 감지 못함
            else
            {
                nextState = 0;

                initialInstNum = stateVector(1);
                finalInstNum = stateVector(1);

                initialT = t(0);
                finalT = t(1);
            }
        }
    }
    // 타격으로 끝남
    else
    {
        // 다음 타격 감지
        if (detectHit)
        {
            nextState = 3;

            initialInstNum = checkOpenHihat(inst(0), hihat(0));
            finalInstNum = detectInst;

            initialT = t(0);
            finalT = detectTime;
        }
        // 다음 타격 감지 못함
        else
        {
            nextState = 1;

            initialInstNum = checkOpenHihat(inst(0), hihat(0));
            finalInstNum = checkOpenHihat(inst(0), hihat(0));

            initialT = t(0);
            finalT = t(1);
        }
    }

    initialInstrument(instrumentMapping[initialInstNum]) = 1.0;
    finalInstrument(instrumentMapping[finalInstNum]) = 1.0;
    outputVector << initialT, initialInstrument, finalT, finalInstrument;

    nextStateVector.resize(3);
    nextStateVector << initialT, initialInstNum, nextState;

    // std::cout << "\n insti -> instf : " << initialInstNum << " -> " << finalInstNum;
    // std::cout << "\n ti -> tf : " << initialT << " -> " << finalT;
    // std::cout << "\n state : " << nextStateVector.transpose();
    // std::cout << "\n";

    return std::make_pair(outputVector, nextStateVector);
}

int PathManager::checkOpenHihat(int instNum, int isHihat)
{
    if (instNum == 5)       // 하이햇인 경우
    {
        if (isHihat == 0)  // 오픈
        {
            return 9;
        }
        else            // 클로즈
        {
            return instNum;
        }
    }
    else                // 하이햇이 아니면 그냥 반환
    {
        return instNum;
    }
}

pair<VectorXd, double> PathManager::getTargetPosition(VectorXd &inst, char RL)
{
    // inst 벡터를 (x,y,z) position으로 변환 
    double angle = 0.0;
    VectorXd p(3);

    if (inst.sum() == 0)
    {
        std::cout << "Instrument Vector Error!! : " << inst << "\n";
    }

    if (RL == 'R' || RL == 'r')
    {
        MatrixXd productP = drumCoordinateR * inst;
        p = productP.block(0, 0, 3, 1);

        MatrixXd productA = wristAngleOnImpactR * inst;
        angle = productA(0, 0);
    }
    else if (RL == 'L' || RL == 'l')
    {
        MatrixXd productP = drumCoordinateL * inst;
        p = productP.block(0, 0, 3, 1);

        MatrixXd productA = wristAngleOnImpactL * inst;
        angle = productA(0, 0);
    }
    else
    {
        std::cout << "RL Error!! : " << RL << "\n";
    }

    return std::make_pair(p, angle);
}

double PathManager::calTimeScaling(double ti, double tf, double t)
{
    // 3차 다항식
    float s;

    MatrixXd A;
    MatrixXd b;
    MatrixXd A_1;
    MatrixXd sol;

    A.resize(4, 4);
    b.resize(4, 1);

    A << 1, ti, ti * ti, ti * ti * ti,
        1, tf, tf * tf, tf * tf * tf,
        0, 1, 2 * ti, 3 * ti * ti,
        0, 1, 2 * tf, 3 * tf * tf;

    b << 0, 1, 0, 0;

    A_1 = A.inverse();
    sol = A_1 * b;

    s = sol(0, 0) + sol(1, 0) * t + sol(2, 0) * t * t + sol(3, 0) * t * t * t;

    return s;
}

VectorXd PathManager::makeTaskSpacePath(VectorXd &Pi, VectorXd &Pf, double s)
{
    float degree = 2.0;

    float xi = Pi(0), xf = Pf(0);
    float yi = Pi(1), yf = Pf(1);
    float zi = Pi(2), zf = Pf(2);

    VectorXd Ps;
    Ps.resize(3);

    if (Pi == Pf)
    {
        Ps(0) = xi;
        Ps(1) = yi;
        Ps(2) = zi;
    }
    else
    {
        Ps(0) = xi + s * (xf - xi);
        Ps(1) = yi + s * (yf - yi);

        if (zi > zf)
        {
            float a = zf - zi;
            float b = zi;

            Ps(2) = a * std::pow(s, degree) + b;
        }
        else
        {
            float amp = abs(zi - zf) / 2;    // sin 함수 amplitude
            float a = (zi - zf) * std::pow(-1, degree);
            float b = zf;

            Ps(2) = a * std::pow(s - 1, degree) + b + (amp * sin(M_PI * s));    // sin 궤적 추가
            // Ps(2) = a * std::pow(s - 1, degree) + b;
        }
    }

    return Ps;
}

VectorXd PathManager::getWaistParams(VectorXd &pR, VectorXd &pL, double theta7, double theta8)
{
    std::vector<VectorXd> Qarr;
    int j = 0;      // 솔루션 개수
    VectorXd output(3);

    VectorXd W(2);
    W << 2, 1;
    double minCost = W.sum();
    double w = 0, cost = 0;
    int minIndex = 0;
    
    for (int i = 0; i < 1801; i++)
    {
        double theta0 = -0.5 * M_PI + M_PI / 1800.0 * i; // the0 범위 : -90deg ~ 90deg
        bool printError = false;

        VectorXd sol = solveGeometricIK(pR, pL, theta0, theta7, theta8, printError);

        if (sol(9) == 0.0)      // 해가 구해진 경우 (err == 0)
        {
            Qarr.push_back(sol);
            j++;
        }
    }

    if (j == 0)
    {
        std::cout << "Error : Waist Range (IK is not solved!!)\n";
        state.main = Main::Error;

        output(0) = 0;
        output(1) = 0;
        output(2) = 0;
    }
    else
    {
        VectorXd minSol = Qarr[0];
        VectorXd maxSol = Qarr[j-1];
        
        // 최적화
        w = 2.0 * M_PI / abs(maxSol[0] - minSol[0]);

        for (int i = 0; i < j; i++)
        {
            VectorXd sol = Qarr[i];
            // 양 팔의 관절각 합이 180도에 가까운 값 선택
            // solution set 중 가운데 값 선택
            cost = W(0)*cos(sol[1] + sol[2]) + W(1)*cos(w*abs(sol[0] - minSol[0]));

            if (cost < minCost)
            {
                minCost = cost;
                minIndex = i;
            }
        }

        VectorXd optSol = Qarr[minIndex];
        
        output(0) = minSol[0];
        output(1) = maxSol[0];
        output(2) = optSol[0];
    }

    return output;
}

void PathManager::storeWaistParams(int n, VectorXd &waistParams)
{
    WaistParameter WP;
    WP.n = n;
    WP.min_q0 = waistParams(0);
    WP.max_q0 = waistParams(1);
    WP.optimized_q0 = waistParams(2);

    waistParameterQueue.push(WP);
}

////////////////////////////////////////////////////////////////////////////////
/*                          Play : Trajectory (Hit)                           */
////////////////////////////////////////////////////////////////////////////////

void PathManager::genHitTrajectory(MatrixXd &measureMatrix, int n)
{
    MatrixXd dividedMatrix;
    HitData hitData;
    double t1 = 0.0;
    double dt = canManager.DTSECOND;
    double timeStep = 0.05;                 // 악보를 쪼개는 단위
    double lineTime = measureMatrix(1,1);   // 궤적 생성하기 위한 한 줄의 시간
    int samplesPerLine = static_cast<int>(round(lineTime / timeStep)); // 한 줄을 쪼갠 개수
    
    // divide
    dividedMatrix = divideMatrix(measureMatrix);

    int k = 0;
    for (int i = 0; i < n; i++)
    {
        if (i >= k*n/samplesPerLine)
        {
            k++;
            if (i != 0)
            {
                // 읽은 줄 삭제
                MatrixXd tmpMatrix(dividedMatrix.rows() - 1, dividedMatrix.cols());
                tmpMatrix = dividedMatrix.block(1, 0, tmpMatrix.rows(), tmpMatrix.cols());
                dividedMatrix.resize(tmpMatrix.rows(), tmpMatrix.cols());
                dividedMatrix = tmpMatrix;
            }

            // 타격 시간
            hitData = getHitData(dividedMatrix, hitStateR, hitStateL);
            
            // state update
            hitStateR = hitData.nextStateR;
            hitStateL = hitData.nextStateL;

            // 타격 궤적 생성
            makeHitCoefficient(hitData, hitStateR, hitStateL);

            t1 = dividedMatrix(0, 8);
        }

        HitTrajectory HT;

        // 시간 변환
        double tHitR = dt * (i - (k-1)*n/samplesPerLine) + t1 - hitData.initialTimeR;
        double tHitL = dt * (i - (k-1)*n/samplesPerLine) + t1 - hitData.initialTimeL;

        HT.elbowR = getElbowAngle(tHitR, elbowTimeR, elbowCoefficientR);
        HT.elbowL = getElbowAngle(tHitL, elbowTimeL, elbowCoefficientL);
        HT.wristR = getWristAngle(tHitR, wristTimeR, wristCoefficientR);
        HT.wristL = getWristAngle(tHitL, wristTimeL, wristCoefficientL);

        HT.kpRatioR = getKpRatio(tHitR, wristTimeR);
        HT.kpRatioL = getKpRatio(tHitL, wristTimeL);

        hitQueue.push(HT);

        // // 데이터 저장
        // std::string fileName;
        // fileName = "hit_angle";
        // func.appendToCSV(fileName, false, HT.elbowR, HT.elbowL, HT.wristR, HT.wristL);
        // fileName = "hit_time";
        // func.appendToCSV(fileName, false, tHitR, tHitL);
    }
}

MatrixXd PathManager::divideMatrix(MatrixXd &measureMatrix)
{
    double timeStep = 0.05;
    int numCols = measureMatrix.cols();
    std::vector<Eigen::VectorXd> newRows;
    newRows.push_back(prevLine);    // 첫 줄 저장

    double prevTotalTime = prevLine(8);

    for (int i = 0; i < measureMatrix.rows() - 1; ++i)
    {
        double duration = measureMatrix(i + 1, 1);                  // 악보 한 줄 시간
        double endTime = measureMatrix(i + 1, 8);                   // 종료 시간 (누적)
        int steps = static_cast<int>(round(duration / timeStep));   // 쪼개진 개수
        double sumStep = (endTime-prevTotalTime) / steps;           // 한 스텝 시간

        for (int s = 0; s < steps; ++s)
        {
            Eigen::VectorXd row(numCols);
            row.setZero();

            row(0) = measureMatrix(i + 1, 0);                 
            row(1) = timeStep;                              
            row(8) = prevTotalTime + (s + 1) * sumStep;     // 누적 시간

            if (s + 1 == steps) // 마지막에 line 정보 그대로 기록
            {
                for (int j = 2; j <= 7; ++j)
                {
                    row(j) = measureMatrix(i + 1, j);
                }
            }

            newRows.push_back(row);
        }

        prevTotalTime = endTime;

    }

    // vector를 Eigen::MatrixXd로 변환
    Eigen::MatrixXd parsedMatrix(newRows.size(), numCols);
    for (size_t i = 0; i < newRows.size(); ++i)
    {
        parsedMatrix.row(i) = newRows[i];
    }

    prevLine = measureMatrix.row(1);

    return parsedMatrix;
}

PathManager::HitData PathManager::getHitData(MatrixXd &measureMatrix, VectorXd &stateR, VectorXd &stateL)
{
    HitData hitData;

    VectorXd t = measureMatrix.col(8);
    VectorXd hitR = measureMatrix.col(4);
    VectorXd hitL = measureMatrix.col(5);
    VectorXd nextStateR, nextStateL; 

    double hitDetectionThreshold = 0.5;     // 다음 타격 감지할 최대 시간
    double preTR = stateR(0), preStateR = stateR(1);
    double preTL = stateL(0), preStateL = stateL(1);

    // parse
    VectorXd dataR = parseHitData(t, hitR, preTR, preStateR, hitDetectionThreshold);
    VectorXd dataL = parseHitData(t, hitL, preTL, preStateL, hitDetectionThreshold);

    nextStateR.resize(3);
    nextStateR << dataR(0), dataR(2), dataR(3);

    nextStateL.resize(3);
    nextStateL << dataL(0), dataL(2), dataL(3);

    hitData.initialTimeR = dataR(0);
    hitData.finalTimeR = dataR(1);
    hitData.initialTimeL = dataL(0);
    hitData.finalTimeL = dataL(1);
    hitData.nextStateR = nextStateR;
    hitData.nextStateL = nextStateL;

    return hitData;
}

VectorXd PathManager::parseHitData(VectorXd &t, VectorXd &hit, double preT, double preState, double hitDetectionThreshold)
{
    VectorXd output;
    double t1, t2, state;
    bool detectHit = false;
    double hitTime = t(1);
    int intensity = 0;

    // 다음 타격 찾기
    for (int i = 1; i < t.rows(); i++)
    {
        if (round(10000 * hitDetectionThreshold) < round(10000 * (t(i) - t(0))))
        {
            if (i != 1)     // 첫 줄은 무조건 읽도록
            {
                break;
            }
        }

        if (hit(i) != 0)
        {
            detectHit = true;
            hitTime = t(i);
            intensity = hit(i);
            break;
        }
    }

    if (hit(0) == 0)  // 현재 줄이 타격이 아님
    {
        if (preState == 2 || preState == 3) // 이전에 타격 O
        {
            t1 = preT;
            t2 = hitTime;
            state = preState;
        }
        else    // 이전에 타격 X
        {
            if (detectHit)
            {
                t1 = t(0);
                t2 = hitTime;
                state = 2;
            }
            else
            {
                t1 = t(0);
                t2 = t(1);
                state = 0;
            }
        }
    }
    else
    {
        // 다음 타격 찾기
        if (detectHit)
        {
            t1 = t(0);
            t2 = hitTime;
            state = 3;
        }
        else
        {
            t1 = t(0);
            t2 = t(1);
            state = 1;
        }
    }

    output.resize(4);
    output << t1, t2, state, intensity;

    return output;
}

void PathManager::makeHitCoefficient(HitData hitData, VectorXd &stateR, VectorXd &stateL)
{
    int intensityR = stateR(2);
    int intensityL = stateL(2);

    // 타격 관련 파라미터
    ElbowAngle elbowAngleR, elbowAngleL;
    WristAngle wristAngleR, wristAngleL;

    elbowTimeR = getElbowTimeParam(hitData.initialTimeR, hitData.finalTimeR, intensityR);
    elbowTimeL = getElbowTimeParam(hitData.initialTimeL, hitData.finalTimeL, intensityL);

    wristTimeR = getWristTimeParam(hitData.initialTimeR, hitData.finalTimeR, intensityR, stateR(1));
    wristTimeL = getWristTimeParam(hitData.initialTimeL, hitData.finalTimeL, intensityL, stateL(1));

    elbowAngleR = getElbowAngleParam(hitData.initialTimeR, hitData.finalTimeR, intensityR);
    elbowAngleL = getElbowAngleParam(hitData.initialTimeL, hitData.finalTimeL, intensityL);

    wristAngleR = getWristAngleParam(hitData.initialTimeR, hitData.finalTimeR, intensityR);
    wristAngleL = getWristAngleParam(hitData.initialTimeL, hitData.finalTimeL, intensityL);

    // 계수 행렬 구하기
    elbowCoefficientR = makeElbowCoefficient(stateR(1), elbowTimeR, elbowAngleR);
    elbowCoefficientL = makeElbowCoefficient(stateL(1), elbowTimeL, elbowAngleL);

    wristCoefficientR = makeWristCoefficient(stateR(1), wristTimeR, wristAngleR);
    wristCoefficientL = makeWristCoefficient(stateL(1), wristTimeL, wristAngleL);
}

PathManager::ElbowTime PathManager::getElbowTimeParam(double t1, double t2, int intensity)
{
    ElbowTime elbowTime;
    float T = t2 - t1; // 전체 타격 시간

    elbowTime.liftTime = std::max(0.5 * (T), T - 0.2);
    elbowTime.hitTime = T;
    
    return elbowTime;
}

PathManager::WristTime PathManager::getWristTimeParam(double t1, double t2, int intensity, int state)
{
    WristTime wristTime;
    float T = t2 - t1;  // 전체 타격 시간

    // 타격 후 복귀 시간
    wristTime.releaseTime = std::min(0.2 * (T), 0.1);   // 이제 의미 없음

    if (state == 2)
    {
        wristTime.liftTime = std::max(0.7 * (T), T - 0.2);      // 최고점에 도달하는 시간
        wristTime.stayTime = 0.4 * T;                          // 상승하기 시작하는 시간
    }
    else
    {
        // state 1 or 3일 때 (복귀 모션 필요)
        // state 3일 때 시간이 0.3초 이하이면 전체 타격 시간의 절반을 기준으로 들고 내리는 궤적(stay 없음)
        if (T <= 0.5)
        {
            wristTime.liftTime = 0.5 * (T);
        }
        else
        {
            // wristTime.liftTime = std::max(0.6 * (T), T - 0.2);
            wristTime.liftTime = T - 0.25;
        }
        wristTime.stayTime = 0.5 * (T);
    }

    wristTime.hitTime = T;
    
    return wristTime;
}

PathManager::ElbowAngle PathManager::getElbowAngleParam(double t1, double t2, int intensity)
{
    ElbowAngle elbowAngle;
    float T = (t2 - t1);        // 전체 타격 시간

    double intensityFactor;  // 1: 0%, 2: 0%, 3: 0%, 4: 90%, 5: 100%, 6: 110%, 7: 120%  

    if (intensity <= 3)
    {
        intensityFactor = 0;
    }
    else
    {
        intensityFactor = 0.1 * intensity + 0.5;  
    }

    if (T < 0.2)
    {
        // 0.2초보다 짧을 땐 안 들게
        elbowAngle.liftAngle = 0;
    }
    else if (T <= 0.5)
    {
        // 0.2초 ~ 0.5초에선 시간에 따라 선형적으로 줄여서 사용
        elbowAngle.liftAngle = (T) * (15 * M_PI / 180.0) / 0.5;
    }
    else
    {
        elbowAngle.liftAngle = 15 * M_PI / 180.0;
    }

    elbowAngle.liftAngle = elbowAngle.stayAngle + elbowAngle.liftAngle * intensityFactor;

    return elbowAngle;
}

PathManager::WristAngle PathManager::getWristAngleParam(double t1, double t2, int intensity)
{
    WristAngle wristAngle;
    float T = (t2 - t1);        // 타격 전체 시간
    double intensityFactor;
   
    if (intensity < 5)
    {
        intensityFactor = 0.25 * intensity - 0.25;  // 1: 0%, 2: 25%, 3: 50%, 4: 75%
    }
    else
    {
        intensityFactor = intensity / 3.0 - (2.0 / 3.0);  // 5: 100%, 6: 133%, 7: 167%
    }

    // Lift Angle (최고점 각도) 계산, 최대 40도 * 세기
    T < 0.5 ? wristAngle.liftAngle = (40.0 * T + 10.0) * M_PI / 180.0 : wristAngle.liftAngle = 30.0 * M_PI / 180.0;

    if (intensity == 1)
    {
        // intensity 1일 땐 아예 안들도록
        wristAngle.liftAngle = wristAngle.stayAngle;
    }
    else
    {
        wristAngle.liftAngle = wristAngle.stayAngle + wristAngle.liftAngle * intensityFactor;
    }
    
    return wristAngle;
}

MatrixXd PathManager::makeElbowCoefficient(int state, ElbowTime eT, ElbowAngle eA)
{
    MatrixXd elbowCoefficient;
    
    MatrixXd A;
    MatrixXd b;
    MatrixXd A_1;
    MatrixXd sol, sol2;

    if (state == 0)
    {
        // Stay
        elbowCoefficient.resize(2, 4);
        elbowCoefficient << eA.stayAngle, 0, 0, 0,  // Stay
                            eA.stayAngle, 0, 0, 0;  // Stay
    }
    else if (state == 1)
    {
        // Release
        if (eA.stayAngle == 0)
        {
            sol.resize(4, 1);
            sol << 0, 0, 0, 0;
        }
        else
        {
            A.resize(4, 4);
            b.resize(4, 1);

            A << 1, 0, 0, 0,
                1, eT.hitTime, eT.hitTime * eT.hitTime, eT.hitTime * eT.hitTime * eT.hitTime,
                0, 1, 0, 0,
                0, 1, 2 * eT.hitTime, 3 * eT.hitTime * eT.hitTime;

            b << 0, eA.stayAngle, 0, 0;

            A_1 = A.inverse();
            sol = A_1 * b;
        }

        elbowCoefficient.resize(2, 4);
        elbowCoefficient << sol(0), sol(1), sol(2), sol(3), // Release
                            sol(0), sol(1), sol(2), sol(3); // Release
    }
    else if (state == 2)
    {
        // Lift
        A.resize(4, 4);
        b.resize(4, 1);

        if(eA.liftAngle == eA.stayAngle)
        {
            sol.resize(4,1);
            sol << eA.stayAngle, 0, 0, 0;
        }
        else
        {
            A << 1, 0, 0, 0,
            1, eT.liftTime, eT.liftTime * eT.liftTime, eT.liftTime * eT.liftTime * eT.liftTime,
            0, 1, 0, 0,
            0, 1, 2 * eT.liftTime, 3 * eT.liftTime * eT.liftTime;

            b << eA.stayAngle, eA.liftAngle, 0, 0;

            A_1 = A.inverse();
            sol = A_1 * b;
        }

        // Hit
        if (eA.liftAngle == eA.stayAngle)
        {
            sol2.resize(4, 1);
            sol2 << 0, 0, 0, 0;
        }
        else
        {
            A.resize(4, 4);
            b.resize(4, 1);

            A << 1, eT.liftTime, eT.liftTime * eT.liftTime, eT.liftTime * eT.liftTime * eT.liftTime,
                1, eT.hitTime, eT.hitTime * eT.hitTime, eT.hitTime * eT.hitTime * eT.hitTime,
                0, 1, 2 * eT.liftTime, 3 * eT.liftTime * eT.liftTime,
                0, 1, 2 * eT.hitTime, 3 * eT.hitTime * eT.hitTime;

            b << eA.liftAngle, 0, 0, 0;

            A_1 = A.inverse();
            sol2 = A_1 * b;
        }

        elbowCoefficient.resize(2, 4);
        elbowCoefficient << sol(0), sol(1), sol(2), sol(3),     // Lift
                            sol2(0), sol2(1), sol2(2), sol2(3); // Hit
    }
    else if (state == 3)
    {
        // Lift
        if (eA.liftAngle == eA.stayAngle)
        {
            sol.resize(4, 1);
            sol << 0, 0, 0, 0;

            sol2.resize(4, 1);
            sol2 << 0, 0, 0, 0;
        }
        else
        {
            A.resize(4, 4);
            b.resize(4, 1);

            A << 1, 0, 0, 0,
                1, eT.liftTime, eT.liftTime * eT.liftTime, eT.liftTime * eT.liftTime * eT.liftTime,
                0, 1, 0, 0,
                0, 1, 2 * eT.liftTime, 3 * eT.liftTime * eT.liftTime;

            b << 0, eA.liftAngle, 0, 0;

            A_1 = A.inverse();
            sol = A_1 * b;

            // Hit
            A.resize(4, 4);
            b.resize(4, 1);

            A << 1, eT.liftTime, eT.liftTime * eT.liftTime, eT.liftTime * eT.liftTime * eT.liftTime,
                1, eT.hitTime, eT.hitTime * eT.hitTime, eT.hitTime * eT.hitTime * eT.hitTime,
                0, 1, 2 * eT.liftTime, 3 * eT.liftTime * eT.liftTime,
                0, 1, 2 * eT.hitTime, 3 * eT.hitTime * eT.hitTime;

            b << eA.liftAngle, 0, 0, 0;

            A_1 = A.inverse();
            sol2 = A_1 * b;
        }

        elbowCoefficient.resize(2, 4);
        elbowCoefficient << sol(0), sol(1), sol(2), sol(3),     // Lift
                            sol2(0), sol2(1), sol2(2), sol2(3); // Hit
    }

    return elbowCoefficient;
}

MatrixXd PathManager::makeWristCoefficient(int state, WristTime wT, WristAngle wA)
{
    MatrixXd wristCoefficient;
    
    MatrixXd A;
    MatrixXd b;
    MatrixXd A_1;
    MatrixXd sol, sol2;

    if (state == 0)
    {
        // Stay
        wristCoefficient.resize(4, 4);
        wristCoefficient << wA.stayAngle, 0, 0, 0,  // Stay
                            wA.stayAngle, 0, 0, 0,  // Stay
                            wA.stayAngle, 0, 0, 0,  // Stay
                            wA.stayAngle, 0, 0, 0;  // Stay
    }
    else if (state == 1)
    {
        // Release
        if (wA.pressAngle == wA.stayAngle)
        {
            sol.resize(4, 1);
            sol << wA.stayAngle, 0, 0, 0;
        }
        else
        {
            A.resize(3, 3);
            b.resize(3, 1);

            A << 1, 0, 0,
                1, wT.hitTime, wT.hitTime * wT.hitTime,
                0, 1, 2 * wT.hitTime;

            b << wA.pressAngle, wA.stayAngle, 0;

            A_1 = A.inverse();
            sol = A_1 * b;
        }

        wristCoefficient.resize(4, 4);
        wristCoefficient << sol(0), sol(1), sol(2), 0,  // Release
                            sol(0), sol(1), sol(2), 0,  // Release
                            sol(0), sol(1), sol(2), 0,  // Release
                            sol(0), sol(1), sol(2), 0;  // Release
    }
    else if (state == 2)
    {
        // Stay - Lift - Hit

        // Lift
        A.resize(4, 4);
        b.resize(4, 1);

        // Lift Angle = Stay Angle일 때(아주 작게 칠 때) -> 드는 궤적 없이 내리기만
        if (wA.stayAngle == wA.liftAngle)
        {
            sol.resize(4, 1);
            sol << wA.stayAngle, 0, 0, 0;
        }
        else
        {
            A << 1, wT.stayTime, wT.stayTime * wT.stayTime, wT.stayTime * wT.stayTime * wT.stayTime,
            1, wT.liftTime, wT.liftTime * wT.liftTime, wT.liftTime * wT.liftTime * wT.liftTime,
            0, 1, 2 * wT.stayTime, 3 * wT.stayTime * wT.stayTime,
            0, 1, 2 * wT.liftTime, 3 * wT.liftTime * wT.liftTime;

            b << wA.stayAngle, wA.liftAngle, 0, 0;

            A_1 = A.inverse();
            sol = A_1 * b;
        }

        // Hit
        if (wA.liftAngle == wA.pressAngle)
        {
            sol2.resize(4, 1);
            sol2 << wA.liftAngle, 0, 0, 0;
        }
        else
        {
            A.resize(3, 3);
            b.resize(3, 1);

            A << 1, wT.liftTime, wT.liftTime * wT.liftTime,
                1, wT.hitTime, wT.hitTime * wT.hitTime,
                0, 1, 2 * wT.liftTime;

            b << wA.liftAngle, wA.pressAngle, 0;

            A_1 = A.inverse();
            sol2 = A_1 * b;
        }

        wristCoefficient.resize(4, 4);
        wristCoefficient << wA.stayAngle, 0, 0, 0,          // Stay
                            wA.stayAngle, 0, 0, 0,          // Stay
                            sol(0), sol(1), sol(2), sol(3), // Lift
                            sol2(0), sol2(1), sol2(2), 0;   // Hit
    }
    else if (state == 3)
    {
        // Lift - Hit

        // Lift
        if (wA.pressAngle == wA.liftAngle)
        {
            sol.resize(4, 1);
            sol << wA.liftAngle, 0, 0, 0;
        }
        else
        {
            A.resize(3, 3);
            b.resize(3, 1);

            A << 1, 0, 0,
                1, wT.stayTime, wT.stayTime * wT.stayTime,
                0, 1, 2 * wT.stayTime;

            b << wA.pressAngle, wA.liftAngle, 0;

            A_1 = A.inverse();
            sol = A_1 * b;
        }

        // Hit
        if (wA.liftAngle == wA.pressAngle)
        {
            sol2.resize(4, 1);
            sol2 << wA.pressAngle, 0, 0, 0;
        }
        else
        {
            A.resize(3, 3);
            b.resize(3, 1);

            A << 1, wT.liftTime, wT.liftTime * wT.liftTime,
                1, wT.hitTime, wT.hitTime * wT.hitTime,
                0, 1, 2 * wT.liftTime;

            b << wA.liftAngle, wA.pressAngle, 0;

            A_1 = A.inverse();
            sol2 = A_1 * b;
        }

        wristCoefficient.resize(4, 4);
        wristCoefficient << sol(0), sol(1), sol(2), 0,      // Lift
                            sol(0), sol(1), sol(2), 0,      // Lift
                            wA.liftAngle, 0, 0, 0,          // Stay (Lift Angle)
                            sol2(0), sol2(1), sol2(2), 0;   // Hit
    }

    return wristCoefficient;
}

double PathManager::getElbowAngle(double t, ElbowTime eT, MatrixXd &coefficientMatrix)
{
    MatrixXd tMatrix;
    tMatrix.resize(4, 1);
    tMatrix << 1, t, t*t, t*t*t;

    MatrixXd elbowAngle = coefficientMatrix * tMatrix;

    if (t < eT.liftTime)
    {
        return elbowAngle(0);
    }
    else
    {
        return elbowAngle(1);
    }
}

double PathManager::getWristAngle(double t, WristTime wT, MatrixXd &coefficientMatrix)
{
    MatrixXd tMatrix;
    tMatrix.resize(4, 1);
    tMatrix << 1, t, t*t, t*t*t;

    MatrixXd wristAngle = coefficientMatrix * tMatrix;

    if (t < wT.releaseTime)
    {
        return wristAngle(0);
    }
    else if (t < wT.stayTime)
    {
        return wristAngle(1);
    }
    else if (t < wT.liftTime)
    {
        return wristAngle(2);
    }
    else
    {
        return wristAngle(3);
    }
}

double PathManager::getKpRatio(double t, WristTime wT)
{
    // 1. Kp 값을 조절할 전체 시간 구간인지 확인합니다.
    if (t > wT.liftTime && t <= wT.hitTime)
    {
        // 2. 전체 구간의 지속 시간(duration)과 
        //    그 절반이 되는 시점(midPointTime)을 계산합니다.
        double duration = wT.hitTime - wT.liftTime;
        double midPointTime = wT.liftTime + (duration / 2.0);

        // 3. 아직 구간의 전반부일 경우 (절반 시점 포함)
        if (t <= midPointTime)
        {
            // Kp 비율을 1.0으로 유지합니다.
            return 1.0;
        }
        // 4. 구간의 후반부일 경우
        else
        {
            // "후반부"만의 지속 시간 (전체 duration의 절반)
            double decreaseDuration = wT.hitTime - midPointTime;
            
            // "후반부" 내에서의 진행도 (0.0 ~ 1.0)
            // (t가 midPointTime일 때 0.0, t가 hitTime일 때 1.0이 됨)
            double decreaseProgress = (t - midPointTime) / decreaseDuration;

            // 1.0에서 kpMin 값까지 선형적으로 감소시킵니다.
            return 1.0 + (kpMin - 1.0) * decreaseProgress;
        }
    }

    // 5. Kp 값을 조절하는 구간이 아니면 1.0을 반환합니다.
    return 1.0;
}

////////////////////////////////////////////////////////////////////////////////
/*                         Play : Trajectory (Pedal)                          */
////////////////////////////////////////////////////////////////////////////////

void PathManager::genPedalTrajectory(MatrixXd &measureMatrix, int n)
{
    double dt = canManager.DTSECOND;
    double t1 = measureMatrix(0, 8);
    double t2 = measureMatrix(1, 8);

    // Bass 관련 변수
    bool bassHit = measureMatrix(0, 6);
    bool nextBaseHit = measureMatrix(1, 6);
    int bassState = getBassState(bassHit, nextBaseHit);
    BassTime bassTime = getBassTime(t1, t2);

    // Hihat 관련 변수
    bool hihatClosed = measureMatrix(0, 7);
    int nextHihatClosed = measureMatrix(1, 7);
    int hihatState = getHihatState(hihatClosed, nextHihatClosed);
    HihatTime hihatTime = getHihatTime(t1, t2);
    
    for (int i = 0; i < n; i++)
    {
        PedalTrajectory PT;

        // 양 발 모터 각도
        PT.bass = getBassAngle(i * dt, bassTime, bassState);
        PT.hihat = getHihatAngle(i * dt, hihatTime, hihatState, nextHihatClosed);
        
        pedalQueue.push(PT);

        // // 데이터 저장
        // std::string fileName;
        // fileName = "pedal_angle";
        // func.appendToCSV(fileName, false, PT.bass, PT.hihat);
    }
}

int PathManager::getBassState(bool bassHit, bool nextBaseHit)
{
    int state = 0;

    if(!bassHit && !nextBaseHit)
    {
        state = 0;
    }
    else if (bassHit && !nextBaseHit)
    {
        state = 1;
    }
    else if (!bassHit && nextBaseHit)
    {
        state = 2;
    }
    else if (bassHit && nextBaseHit)
    {
        state = 3;
    }

    return state;
}

PathManager::BassTime PathManager::getBassTime(float t1, float t2)
{
    BassTime bassTime;
    float T = t2 - t1;      // 전체 타격 시간
    
    bassTime.liftTime = std::max(0.6 * (T), T - 0.2);   // 최고점 시간, 전체 타격 시간의 60% , 타격 시간의 최대값은 0.2초
    bassTime.stayTime = 0.45 * (T);     // 복귀 시간, 전체 타격 시간의 45%
    
    bassTime.hitTime = T;

    return bassTime;
}

int PathManager::getHihatState(bool hihatClosed, bool nextHihatClosed)
{
    int state = 0;

    if(!hihatClosed && !nextHihatClosed)
    {
        state = 0;
    }
    else if (hihatClosed && !nextHihatClosed)
    {
        state = 1;
    }
    else if (!hihatClosed && nextHihatClosed)
    {
        state = 2;
    }
    else if (hihatClosed && nextHihatClosed)
    {
        state = 3;
    }

    return state;
}

PathManager::HihatTime PathManager::getHihatTime(float t1, float t2)
{
    float T = t2 - t1;
    HihatTime hihatTime;

    hihatTime.liftTime = 0.1 * T;
    hihatTime.settlingTime = 0.9 * T;
    hihatTime.hitTime = T;
    hihatTime.splashTime = std::max(0.5*T, T - 0.1);       // 0.1초에 20도 이동이 안전. 그 이하는 모터 멈출 수 있음.

    return hihatTime;
}

double PathManager::getBassAngle(double t, BassTime bt, int bassState)
{
    BassAngle bA;
    // Xl : lift 궤적 , Xh : hit 궤적
    double X0 = 0.0, Xp = 0.0, Xl = 0.0, Xh = 0.0;

    X0 = bA.stayAngle;          // 준비 각도
    Xp = bA.pressAngle;         // 최저점 각도

    // 타격 시간이 0.2초 이하일 때
    if (bt.hitTime <= 0.2)
    {
        if (bassState == 3)
        {
            // 짧은 시간에 연속으로 타격하는 경우 덜 들도록, 올라오는 궤적과 내려가는 궤적을 합쳐서 사용 (- 영역이라 가능)
            // 조정수 박사님 자료 BassAngle 부분 참고
            double temp_liftTime = bt.hitTime / 2;
            Xl = -0.5 * (Xp - X0) * (cos(M_PI * (t / bt.hitTime + 1)) - 1);

            if (t < temp_liftTime)
            {
                Xh = 0;
            }
            else 
            {
                Xh = -0.5 * (Xp - X0) * (cos(M_PI * (t - temp_liftTime) / (bt.hitTime - temp_liftTime)) - 1);
            }
        }

        // 시간이 짧을 땐 전체 시간을 다 써서 궤적 생성
        else if (bassState == 1)
        {
            // 복귀하는 궤적
            Xl = -0.5 * (Xp - X0) * (cos(M_PI * (t / bt.hitTime + 1)) - 1);
            Xh = 0;
        }
        else if (bassState == 2)
        {
            // 타격하는 궤적
            Xl = 0;
            Xh = -0.5 * (Xp - X0) * (cos(M_PI * t / bt.hitTime) - 1);
        }
        else
        {
            Xl = 0;
            Xh = 0;
        } 
    }

    // 0.2초 이상일 때 
    // StayTime 이전 -> 타격 후 들어올림
    else if (t < bt.stayTime)
    {
        if(bassState == 1 || bassState == 3)    
        {
            // 복귀하는 궤적
            Xl = -0.5 * (Xp - X0) * (cos(M_PI * (t / bt.stayTime + 1)) - 1);
            Xh = 0;
        }
        else
        {
            Xl = 0;
            Xh = 0;
        }
    }

    // LiftTime부터 HitTime까지 -> 타격
    else if (t > bt.liftTime && t <= bt.hitTime)
    {
        if (bassState == 2 || bassState == 3)
        {
            // 타격하는 궤적
            Xl = 0;
            Xh = -0.5 * (Xp - X0) * (cos(M_PI * (t - bt.liftTime) / (bt.hitTime - bt.liftTime)) - 1);
        }
        else
        {
            Xl = 0;
            Xh = 0;
        }
    }
    
    return X0 + Xl + Xh;        // 준비 각도 + 하강 각도 + 상승 각도
}

double PathManager::getHihatAngle(double t, HihatTime ht, int hihatState, int nextHihatClosed)
{
    // 1.open/closed, 2.splash 두 개의 하이햇 연주법을 구현함.
    // o/c 상태에 따라 악기 소리가 다름. 소리나는 도중에 상태가 변해도 소리가 변함. 
    // 정확한 소리를 내기 위해 타격 이전(settlingTime)에 상태를 바꾸고 타격 이후(liftTime)에도 그 상태를 유지함.
    // splash는 페달을 밟았다가 떼며 두 심벌이 부딪힘으로 소리내는 방식임.
    // nextHihatClosed == 2 이면 splash 연주법임.

    HihatAngle HA;

    // 각도 변경 원할 시 헤더파일에서 해당 각도 수정
    double X0 = HA.openAngle;       // Open Hihat : -3도
    double Xp = HA.closedAngle;     // Closed Hihat : -13도 

    double Xl = 0.0;

    if(hihatState == 0)
    {
        Xl = X0;
    }
    else if(ht.hitTime <= 0.2)       // 한 박의 시간이 0.2초 이하인 경우
    {
        if(hihatState == 1)
        {
            Xl = makeCosineProfile(Xp, X0, 0, ht.hitTime, t);       // 전체 시간동안 궤적 생성
            /*Xl = makeCosineProfile(Xp, X0, 0, 0.9*ht.hitTime, t);     // 타격 이전에 open/closed가 되도록 0.9T
            if(t >= 0.8*ht.hitTime)
            {
                Xl = X0;
            }*/
        }
        else if(nextHihatClosed == 2)      // Hihat splash 궤적
        {
            if(t < ht.splashTime)
            {
                if(hihatState == 2)
                {
                    Xl = makeCosineProfile(X0, Xp, 0, ht.hitTime, t);
                }
                else if(hihatState == 3)
                {
                    Xl = makeCosineProfile(Xp, Xp - (Xp - X0) * ht.hitTime / 0.2, 0, ht.splashTime, t);     // 20도/0.1초 의 속도를 유지하기 위해서 시간에 따라 이동량 감소시킴.
                }
            }
            else
            {
                if(hihatState == 2)
                {
                    Xl = makeCosineProfile(X0, Xp, 0, ht.hitTime, t);
                }
                else if(hihatState == 3)
                {
                    Xl = makeCosineProfile(Xp - (Xp - X0) * ht.hitTime / 0.2, Xp, ht.splashTime, ht.hitTime, t);
                }
            }
        }
        else        // open/closed Hihat 궤적
        {
            if(hihatState == 2)
            {
                Xl = makeCosineProfile(X0, Xp, 0, ht.hitTime, t);
                /*Xl = makeCosineProfile(X0, Xp, 0, 0.9*ht.hitTime, t);
                if(t >= 0.9*ht.hitTime)
                {
                    Xl = Xp;
                }*/
            }
            else if(hihatState == 3)
            {                
                Xl = Xp;
            }
        }
    }
    // 한 박의 시간이 0.2초 초과인 경우
    else if(nextHihatClosed == 2)      // Hi-Hat splash 궤적
    {
        if(t < ht.splashTime)
        {
            if(hihatState == 2)
            {
                Xl = X0;
            }
            else if(hihatState == 3)
            {
                Xl = makeCosineProfile(Xp, X0, 0, ht.splashTime, t);
            }
        }
        else if(t >= ht.splashTime)
        {
            if(hihatState == 2)
            {
                Xl = makeCosineProfile(X0, Xp, ht.splashTime, ht.hitTime, t);
            }
            else if(hihatState == 3)
            {
                Xl = makeCosineProfile(X0, Xp, ht.splashTime, ht.hitTime, t);
            }
        }
    }
    else        // open/closed Hi-Hat 궤적
    {
        if(t < ht.liftTime)
        {
            if(hihatState == 1)
            {
                Xl = Xp;
            }
            else if(hihatState == 2)
            {
                Xl = X0;
            }
            else if(hihatState == 3)
            {                
                Xl = Xp;
            }
        }
        else if(t >= ht.liftTime && t < ht.settlingTime)
        {
            if(hihatState == 1)
            {
                Xl = makeCosineProfile(Xp, X0, ht.liftTime, ht.settlingTime, t);
            }
            else if(hihatState == 2)
            {      
                Xl = makeCosineProfile(X0, Xp, ht.liftTime, ht.settlingTime, t);
            }
            else if(hihatState == 3)
            {
                Xl = Xp;
            }
        }
        else if(t >= ht.settlingTime)
        {
            if(hihatState == 1)
            {
                Xl = X0;
            }
            else if(hihatState == 2)
            {
                Xl = Xp;
            }
            else if(hihatState == 3)
            {
                Xl = Xp;
            }
        }
    }

    return Xl;
}

double PathManager::makeCosineProfile(double qi, double qf, double ti, double tf, double t)
{
    // ti <= t <= tf
    double A, B, w;

    A = (qi - qf) / 2;
    B = (qi + qf) / 2;
    w = M_PI / (tf - ti);

    return A * cos (w*(t - ti)) + B;
}

////////////////////////////////////////////////////////////////////////////////
/*                           Play : Trajectory (DXL)                          */
////////////////////////////////////////////////////////////////////////////////

void PathManager::genDxlTrajectory(MatrixXd &measureMatrix, int n)
{
    // 악기 정보
    static int curInst = 0;
    static int nextInst = 0;
    int codeInst = measureMatrix(1, 2);

    if(codeInst != 0)
    {
        nextInst = codeInst;
    }
    else
    {
        curInst = nextInst;
    }

    // 악기에 맞는 각도 계산
    double curAngle  = getInstAngle(curInst);
    double targetAngle = getInstAngle(nextInst);

    // 고개 세기
    double nodIntensity = getNodIntensity(measureMatrix);

    double beatOfLine = measureMatrix(1, 1) / 0.6;

    for (int i = 0; i < n; i++)
    {
        DXLTrajectory DXL;

        double tau = (double)i / (n - 1); 
        DXL.dxl1 = curAngle + (targetAngle - curAngle) * (3.0 * pow(tau, 2) - 2.0 * pow(tau, 3));
        DXL.dxl2 = makeNod(beatOfLine, nodIntensity, i, n);

        DXLQueue.push(DXL);
    }

    curInst = nextInst;
}

float PathManager::getInstAngle(int Inst)
{
    double inst_x = 0.0;
    double inst_y = 0.0;

    if(Inst == 0)
    {
        inst_x = 0.0;
        inst_y = 1.0;
    }
    else
    {
        // 저장된 좌표 가져오기(x, y)
        inst_x = drumCoordinateR(0,Inst - 1);
        inst_y = drumCoordinateR(1,Inst - 1);
    }

    return static_cast<float>(atan2(inst_x, inst_y));
}

double PathManager::getNodIntensity(MatrixXd &measureMatrix)
{
    VectorXd beat = measureMatrix.col(1);
    VectorXd intensityR = measureMatrix.col(4);
    VectorXd intensityL = measureMatrix.col(5);
    VectorXd bass = measureMatrix.col(6);

    int rows = measureMatrix.rows();
    double beatSum = 0.0;
    double nodIntensity = 0.0;
    int line = 0;

    for (int i = 0; i < rows; i++)
    {
        if (i == 0) continue;   // 2번 줄부터 반영

        double lineIntensity = 0.1 * intensityR(i) + 0.1 * intensityL(i) + 0.5 * bass(i);
        nodIntensity += lineIntensity;
        line++;

        beatSum += beat(i);
        if (beatSum >= 0.6)
        {
            // 한 박 악보만 읽기
            break;
        }
    }

    return std::min(nodIntensity / line, 1.0);
}

float PathManager::makeNod(double beatOfLine, double nodIntensity, int i, int n)
{
    // Nod: 고개를 끄덕이다

    // 각도
    double nodMaxRange = 20*M_PI/180.0; // 고개가 움직이는 최대 각도
    double nodAngle = 0.0;

    // 세기
    // nodIntensity [0 1]
    static double preNodIntensity = 0.0;
    double alpha = (i * nodIntensity + (n - i) * preNodIntensity) / (double)n;

    // beatSum 범위: [0 1)
    static double beatSum = 0.0;
    beatSum += beatOfLine/n;
    beatSum = beatSum>=1.0?beatSum-1.0:beatSum;

    if (beatSum < 0.7)
    {
        // 상승 궤적 생성
        double tau = beatSum / 0.7;
        nodAngle = alpha * nodMaxRange * (3.0 * pow(tau, 2) - 2.0 * pow(tau, 3));
    }
    else
    {   
        // 하강 궤적 생성
        double tau = (beatSum - 0.7) / 0.3;
        nodAngle = alpha * nodMaxRange * (1.0 - (3.0 * pow(tau, 2) - 2.0 * pow(tau, 3)));
    }

    if (i == n - 1)
    {
        preNodIntensity = nodIntensity;
    }

    return static_cast<float>(readyAngle(13) - nodAngle);
}

////////////////////////////////////////////////////////////////////////////////
/*                      Play : Solve IK and Push Command                      */
////////////////////////////////////////////////////////////////////////////////

std::vector<PathManager::WaistParameter> PathManager::waistParamsQueueToVector()
{
    std::vector<WaistParameter> WP;
    int size = waistParameterQueue.size();

    for (int i = 0; i < size; i++)
    {
        WaistParameter temp = waistParameterQueue.front();
        waistParameterQueue.pop();
        waistParameterQueue.push(temp);
        WP.push_back(temp);
    }

    return WP;
}

MatrixXd PathManager::makeWaistCoefficient(std::vector<WaistParameter> &waistParams)
{
    vector<double> m = {0.0, 0.0};
    double waistAngleT2;

    int n = waistParams[0].n;           // 명령 개수
    double dt = canManager.DTSECOND;
    double T = n * dt;    // 전체 시간

    MatrixXd A;
    MatrixXd b;
    MatrixXd A_1;
    MatrixXd waistCoefficient;

    if (waistParams.size() == 1)
    {
        // 마지막 줄 -> 허리 각 유지
        waistAngleT2 = waistAngleT1;
    }
    else
    {
        // waistAngleT2 구하기
        std::pair<double, vector<double>> output = getWaistAngleT2(waistParams);
        waistAngleT2 = output.first;
        m = output.second; 
    }

    A.resize(4, 4);
    b.resize(4, 1);

    A << 1, 0, 0, 0,
        1, T, T * T, T * T * T,
        0, 1, 0, 0,
        0, 1, 2 * T, 3 * T * T;
    b << waistAngleT1, waistAngleT2, m[0], m[1];

    A_1 = A.inverse();
    waistCoefficient = A_1 * b;

    // 값 저장
    waistAngleT0 = waistAngleT1;
    waistAngleT1 = waistAngleT2;
    waistTimeT0 = -1*T;

    return waistCoefficient;
}

std::pair<double, vector<double>> PathManager::getWaistAngleT2(std::vector<WaistParameter> &waistParams)
{
    VectorXd timeVector;     // getWaistAngleT2() 함수 안에서 사용할 시간 벡터
    double dt = canManager.DTSECOND;
    int paramsSize = waistParams.size();
    vector<double> m_interpolation = {0.0, 0.0};
    double waistAngleT2 = 0.0, waistAngleT3;;

    VectorXd a(3);  // 기울기
    double avg_a;   // 기울기 평균
    double q0Min, q0Max;

    // time vector
    timeVector.resize(6);
    for (int i = 0; i < 6; i++)
    {
        if (i == 0)
        {
            timeVector(i) = waistTimeT0;
        }
        else if (i == 1)
        {
            timeVector(i) = 0;
        }
        else if (paramsSize >= i-1)
        {
            timeVector(i) = timeVector(i-1) + waistParams[i-2].n*dt;
        }
        else
        {
            timeVector(i) = timeVector(i-1) + 1;
        }
    }

    // 기울기 평균 이동 : t1 -> t2
    for (int i = 0; i < 3; i++)
    {
        if (paramsSize > i+1)
        {
            a(i) = (waistParams[i+1].optimized_q0 - waistAngleT1) / (timeVector(i+2)-timeVector(1));
        }
        else
        {
            a(i) = (timeVector(i+1)-timeVector(1)) / (timeVector(i+2)-timeVector(1)) * a(i-1);
        }
    }

    avg_a = a.sum()/3.0;
    waistAngleT2 = avg_a*(timeVector(2)-timeVector(1)) + waistAngleT1;

    // 허리 범위 확인
    q0Min = waistParams[1].min_q0;
    q0Max = waistParams[1].max_q0;
    if (waistAngleT2 <= q0Min || waistAngleT2 >= q0Max)
    {
        waistAngleT2 = 0.5*q0Min + 0.5*q0Max;
    }

    // 기울기 평균 이동 : t2 -> t3
    if (paramsSize == 2)
    {
        waistAngleT3 = waistAngleT2;
    }
    else
    {
        for (int i = 0; i < 2; i++)
        {
            if (paramsSize > i+2)
            {
                a(i) = (waistParams[i+2].optimized_q0 - waistAngleT2) / (timeVector(i+3)-timeVector(2));
            }
            else
            {
                a(i) = (timeVector(i+2)-timeVector(2)) / (timeVector(i+3)-timeVector(2)) * a(i-1);
            }
        }

        avg_a = a.sum()/2.0;
        waistAngleT3 = avg_a*(timeVector(3)-timeVector(2)) + waistAngleT2;

        // 허리 범위 확인
        q0Min = waistParams[2].min_q0;
        q0Max = waistParams[2].max_q0;
        if (waistAngleT3 <= q0Min || waistAngleT3 >= q0Max)
        {
            waistAngleT3 = 0.5*q0Min + 0.5*q0Max;
        }
    }

    // 3차 보간법
    vector<double> y = {waistAngleT0, waistAngleT1, waistAngleT2, waistAngleT3};
    vector<double> x = {timeVector(0), timeVector(1), timeVector(2), timeVector(3)};
    m_interpolation = cubicInterpolation(y, x);

    return std::make_pair(waistAngleT2, m_interpolation);
}

vector<double> PathManager::cubicInterpolation(const vector<double> &q, const vector<double> &t)
{
    vector<double> m = {0.0, 0.0};
    vector<double> a(3, 0.0);

    for (int i = 0; i < 3; i++)
    {
        a[i] = (q[i + 1] - q[i]) / (t[i + 1] - t[i]);
    }
    double m1 = 0.5 * (a[0] + a[1]);
    double m2 = 0.5 * (a[1] + a[2]);

    // Monotone Cubic Interpolation 적용
    double alph, bet;
    if (q[1] == q[2])
    {
        m1 = 0;
        m2 = 0;
    }
    else
    {
        if((q[0] == q[1]) || (a[0] * a[1] < 0)){
            m1 = 0;
        }
        else if((q[2] == q[3]) || (a[1] * a[2] < 0)){
            m2 = 0;
        }
        alph = m1 / (q[2] - q[1]);
        bet = m2 / (q[2] - q[1]);

        double e = std::sqrt(std::pow(alph, 2) + std::pow(bet, 2));
        if (e > 3.0)
        {
            m1 = (3 * m1) / e;
            m2 = (3 * m2) / e;
        }
    }
    
    m[0] = m1;
    m[1] = m2;
    
    return m;
}

double PathManager::getWaistAngle(MatrixXd &waistCoefficient, int index)
{
    double dt = canManager.DTSECOND;
    double t = dt * index;

    return waistCoefficient(0, 0) + waistCoefficient(1, 0) * t + waistCoefficient(2, 0) * t * t + waistCoefficient(3, 0) * t * t * t;
}

VectorXd PathManager::getJointAngles(double q0, double &KpRatioR, double &KpRatioL)
{
    VectorXd q(12);

    // x,y,z
    TaskSpaceTrajectory TT;
    TT = taskSpaceQueue.front();
    taskSpaceQueue.pop();

    // IK
    bool printError = true;
    VectorXd sol = solveGeometricIK(TT.trajectoryR, TT.trajectoryL, q0, TT.wristAngleR, TT.wristAngleL, printError);

    if (sol(0) >= 99)   // 해가 존재하지 않음
    {
        std::cout << "Error : No solution (IK is not solved!!)\n";
        state.main = Main::Error;
    }
    
    for (int i = 0; i < 9; i++)
    {
        q(i) = sol(i);
    }

    // 타격
    HitTrajectory HT;
    HT = hitQueue.front();
    hitQueue.pop();

    q(3) += HT.elbowR / 3.0;
    q(5) += HT.elbowL / 3.0;

    q(4) = (q(4)+HT.elbowR)>=(140.0*M_PI/180.0)?140.0*M_PI/180.0:q(4)+HT.elbowR;
    q(6) = (q(6)+HT.elbowL)>=(140.0*M_PI/180.0)?140.0*M_PI/180.0:q(6)+HT.elbowL;
    
    q(7) += HT.wristR;
    q(8) += HT.wristL;
    
    q(9) = 0.0; // test maxon motor

    // 발 모터
    PedalTrajectory PT;
    PT = pedalQueue.front();
    pedalQueue.pop();

    q(10) = PT.bass;
    q(11) = PT.hihat;

    KpRatioR = HT.kpRatioR;
    KpRatioL = HT.kpRatioL;

    return q;
}

void PathManager::pushDxlBuffer(double q0)
{
    DXLTrajectory dxlQ = DXLQueue.front();
    DXLQueue.pop();

    vector<vector<float>> dxlCommand = {{0.0, 0.0, dxlQ.dxl1 + (float)q0}, {0.0, 0.0, dxlQ.dxl2}};
    {
        std::lock_guard<std::mutex> lock(dxlBufferMutex);
        dxlCommandBuffer.push(dxlCommand);
    }
}

void PathManager::pushCommandBuffer(VectorXd &Qi, double kpRatioR, double kpRatioL)
{
    for (auto &entry : motors)
    {
        int can_id = canManager.motorMapping[entry.first];

        if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
        {
            TMotorData newData;
            newData.position = tMotor->jointAngleToMotorPosition(Qi[can_id]);

            if (tmotorMode == "position")
            {
                newData.mode = tMotor->Position;
                newData.velocityERPM = 0.0;
            }
            // else if (tmotorMode == "velocityFF")
            // {
            //     newData.mode = tMotor->VelocityFF;
            //     newData.velocityERPM = tMotor->cwDir * getVelocityRadps(false, Qi[can_id], can_id) * tMotor->pole * tMotor->gearRatio * 60.0 / 2.0 / M_PI;
            // }
            // else if (tmotorMode == "velocityFB")
            // {
            //     newData.mode = tMotor->VelocityFB;
            //     newData.velocityERPM = 0.0;
            // }
            else if (tmotorMode == "velocity")
            {
                newData.mode = tMotor->Velocity;
                newData.velocityERPM = tMotor->cwDir * getVelocityRadps(false, Qi[can_id], can_id) * tMotor->pole * tMotor->gearRatio * 60.0 / 2.0 / M_PI;
            }

            if (can_id == 0)
            {
                float alpha = 0.7;
                float diff = alpha*preDiff + (1 - alpha)*std::abs(newData.position - prevWaistPos);
                prevWaistPos = newData.position;
                newData.useBrake = (diff < 0.01 * M_PI / 180.0) ? 1 : 0;
                preDiff = diff;

                // func.appendToCSV("brake input", false, newData.position, newData.useBrake, diff);
            }
            else
            {
                newData.useBrake = 0;
            }

            {
                std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
                tMotor->commandBuffer.push(newData);
            }

            tMotor->finalMotorPosition = newData.position;
        }
        else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
        {
            // maxon motor 는 1ms 동작 (1차 보간)
            for (int i = 0; i < 5; i++)
            {
                MaxonData newData;
                float Qi1ms = ((i+1)*Qi[can_id] + (4-i)*maxonMotor->pre_q)/5.0;
                newData.position = maxonMotor->jointAngleToMotorPosition(Qi1ms);
                if (can_id == 10 || can_id == 11)
                {
                    // 발 모터는 항상 CSP mode
                    newData.mode = maxonMotor->CSP;
                    newData.kp = 0;
                    newData.kd = 0;
                }
                else if (maxonMode == "CST")
                {
                    newData.mode = maxonMotor->CST;
                    newData.kp = Kp * (can_id==7?kpRatioR:kpRatioL);
                    newData.kd = Kd;
                    if(kpRatioR != 1.0 || kpRatioL != 1.0)
                    {
                        newData.kd = KdDrop;
                        newData.kp = kpMax * (can_id==7?kpRatioR:kpRatioL);
                    }

                }
                else
                {
                    newData.mode = maxonMotor->CSP;
                    newData.kp = 0;
                    newData.kd = 0;
                }
                
                {
                    std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);
                    maxonMotor->commandBuffer.push(newData);
                }

                maxonMotor->finalMotorPosition = newData.position;
            }
            maxonMotor->pre_q = Qi[can_id];
        }
    }
}

float PathManager::getVelocityRadps(bool restart, double q, int can_id)
{
    static double pre_q[7] = {0};
    static double pre_v[7] = {0};
    const double alpha = 0.1;
    const double dt = 0.005;

    if (restart)
    {
        // 연주 시작할 때마다 초기 위치로 초기화
        for (int i = 0; i < 7; i++)
        {
            pre_q[i] = readyAngle(i);
            pre_v[i] = 0.0;
        }
        return 0.0;
    }
    else
    {
        double v = (q - pre_q[can_id]) / dt;
        double v_f = alpha * pre_v[can_id] + (1.0 - alpha) * v; // 1차 필터
        
        pre_q[can_id] = q;
        pre_v[can_id] = v_f;

        return v;
        // return v_f;
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                              Detect Collision                              */
////////////////////////////////////////////////////////////////////////////////

bool PathManager::detectCollision(MatrixXd &measureMatrix)
{
    VectorXd measureTime = measureMatrix.col(8);
    VectorXd measureIntensityR = measureMatrix.col(4);
    VectorXd measureIntensityL = measureMatrix.col(5);

    VectorXd stateDCR = measureStateR;
    VectorXd stateDCL = measureStateL;

    // 충돌 예측을 위한 악보 범위 (목표위치 없는 부분 제거)
    int endIndex = findDetectionRange(measureMatrix);

    // 충돌 예측
    bool isColli = false;
    double stepSize = 5;
    for (int i = 0; i < endIndex-1; i++)
    {
        MatrixXd tmpMatrix = measureMatrix.block(i,0,measureMatrix.rows()-i,measureMatrix.cols());

        TrajectoryData data = getTrajectoryData(tmpMatrix, stateDCR, stateDCL);
        stateDCR = data.nextStateR;
        stateDCL = data.nextStateL;

        double dt = (data.t2 - data.t1)/stepSize;

        for (int j = 0; j < stepSize+1; j++)
        {
            double tR = dt * j + data.t1 - data.initialTimeR;
            double tL = dt * j + data.t1 - data.initialTimeL;

            double sR = calTimeScaling(0.0, data.finalTimeR - data.initialTimeR, tR);
            double sL = calTimeScaling(0.0, data.finalTimeL - data.initialTimeL, tL);
            
            VectorXd PR = makeTaskSpacePath(data.initialPositionR, data.finalPositionR, sR);
            VectorXd PL = makeTaskSpacePath(data.initialPositionL, data.finalPositionL, sL);

            double Tr = 1.0, hitR, hitL;
            if (measureTime(i+1) - measureTime(i) < 0.5)
            {
                Tr = (measureTime(i+1) - measureTime(i))/0.5;
            }
            
            if (measureIntensityR(i+1) == 0)
            {
                hitR = 10.0 * M_PI / 180.0;
            }
            else
            {
                hitR = measureIntensityR(i+1)*Tr*15.0*sin(M_PI*j/stepSize) * M_PI / 180.0;
            }

            if (measureIntensityL(i+1) == 0)
            {
                hitL = 10.0 * M_PI / 180.0;
            }
            else
            {
                hitL = measureIntensityL(i+1)*Tr*15.0*sin(M_PI*j/stepSize) * M_PI / 180.0;
            }

            if (checkTable(PR, PL, hitR, hitL))
            {
                // std::cout << "\n 충돌이 예측됨 \n";
                // std::cout << "\n R :" << PR.transpose() << ", L :" << PL.transpose() << "\n";
                // std::cout << "\n t :" << (measureTime(i+1) - measureTime(i))*j/stepSize + measureTime(i) << "\n";

                // return true;
                isColli = true;
            }
        }

        if (isColli)
        {
            return true;
        }
    }

    return false;
}

int PathManager::findDetectionRange(MatrixXd &measureMatrix)
{
    // 뒤쪽 목표위치 없는 부분 제거
    // endIndex 까지 탐색
    VectorXd measureTime = measureMatrix.col(8);
    VectorXd measureInstrumentR = measureMatrix.col(2);
    VectorXd measureInstrumentL = measureMatrix.col(3);

    bool endR = false, endL = false;
    int endIndex = measureTime.rows();
    double hitDetectionThreshold = 1.2 * 100.0 / bpmOfScore;

    // 뒤에서부터 읽으면서 양 팔 다 목표위치가 있는 인덱스 반환
    for (int i = 0; i < measureTime.rows(); i++)
    {
        if (measureInstrumentR(measureTime.rows() - 1 - i) != 0)
        {
            endR = true;
        }

        if (!endR)
        {
            if (round(10000 * hitDetectionThreshold) < round(10000 * (measureTime(measureTime.rows() - 1) - measureTime(measureTime.rows() - 1 - i))))
            {
                endR = true;
            }
        }

        if (measureInstrumentL(measureTime.rows() - 1 - i) != 0)
        {
            endL = true;
        }

        if (!endL)
        {
            if (round(10000 * hitDetectionThreshold) < round(10000 * (measureTime(measureTime.rows() - 1) - measureTime(measureTime.rows() - 1 - i))))
            {
                endL = true;
            }
        }

        if (endR && endL)
        {
            endIndex = measureTime.rows() - i;
            break;
        }
    }

    return endIndex;
}

bool PathManager::checkTable(VectorXd PR, VectorXd PL, double hitR, double hitL)
{
    double rangeMin[8] = {0.0, 0.0, -0.3, 0.3, 0.5, -0.3, 0.3, 0.5};
    double rangeMax[8] = {50.0*M_PI/180.0, 50.0*M_PI/180.0, 0.5, 0.7, 1.0, 0.5, 0.7, 1.0};

    std::vector<double> target = {hitR, hitL, PR(0), PR(1), PR(2), PL(0), PL(1), PL(2)};
    
    std::vector<size_t> dims = {11, 11, 19, 10, 12, 19, 10, 12};    // Rw Lw Rx Ry Rz Lx Ly Lz
    std::vector<size_t> targetIndex;

    // 인덱스 공간으로 변환
    for (int i = 0; i < 8; i++)
    {
        size_t index = round((dims[i]-1)*(target[i] - rangeMin[i])/(rangeMax[i] - rangeMin[i]));

        if (index > dims[i]-1)
        {
            index = (size_t)(dims[i]-1);
        }
        else if (index < 0)
        {
            index = 0;
        }

        targetIndex.push_back(index);
    }

    // 테이블 확인
    std::ifstream tableFile(tablePath, std::ifstream::binary);
    
    if (tableFile)
    {
        std::size_t offsetIndex = getFlattenIndex(targetIndex, dims);

        std::pair<size_t, size_t> bitIndex = getBitIndex(offsetIndex);
        
        tableFile.seekg(bitIndex.first, std::ios::beg);
        
        char byte;
        tableFile.read(&byte, 1);
        if (!tableFile) {
            std::cerr << " 바이트 읽기 실패 \n";
            return false;
        }

        tableFile.close();

        // byte에서 원하는 2비트 추출 (LSB 기준)
        uint8_t value = (byte >> bitIndex.second) & 0b11;

        if (value == 0b00)
        {
            // func.appendToCSV("CC1", false, targetIndex[0], targetIndex[1], targetIndex[2]);
            // func.appendToCSV("CC2", false, targetIndex[3], targetIndex[4], targetIndex[5]);
            // func.appendToCSV("CC3", false, targetIndex[6], targetIndex[7], 0);
            return false;   // 충돌 안함
        }
        else
        {
            // func.appendToCSV("CC1", false, targetIndex[0], targetIndex[1], targetIndex[2]);
            // func.appendToCSV("CC2", false, targetIndex[3], targetIndex[4], targetIndex[5]);
            // func.appendToCSV("CC3", false, targetIndex[6], targetIndex[7], 1);
            return true;    // 충돌 위험 or IK 안풀림
        }
    }
    else
    {
        std::cout << "\n 테이블 열기 실패 \n";
        std::cout << tablePath;
        return false;
    }
}

size_t PathManager::getFlattenIndex(const std::vector<size_t>& indices, const std::vector<size_t>& dims)
{
    size_t flatIndex = 0;
    size_t multiplier = 1;

    for (int i = 0; i < 8; i++)
    {
        flatIndex += indices[7-i] * multiplier;
        multiplier *= dims[7-i];
    }

    return flatIndex;
}

std::pair<size_t, size_t> PathManager::getBitIndex(size_t offsetIndex)
{
    size_t bitNum = offsetIndex * 2;
    size_t byteNum = bitNum / 8;
    size_t bitIndex = bitNum - 8 * byteNum;

    return std::make_pair(byteNum, bitIndex);
}

////////////////////////////////////////////////////////////////////////////////
/*                              Avoid Collision                               */
////////////////////////////////////////////////////////////////////////////////

bool PathManager::modifyMeasure(MatrixXd &measureMatrix, int priority)
{
    // 주어진 방법으로 회피되면 measureMatrix를 바꾸고 True 반환

    std::string method = modificationMethods[priority];
    MatrixXd modifedMatrix = measureMatrix;
    bool modificationSuccess = true;
    int nModification = 0;  // 주어진 방법을 사용한 횟수

    while (modificationSuccess)
    {
        if (method == modificationMethods[0])
        {
            modificationSuccess = modifyCrash(modifedMatrix, nModification);     // 주어진 방법으로 수정하면 True 반환
        }
        else if (method == modificationMethods[1])
        {
            modificationSuccess = waitAndMove(modifedMatrix, nModification);     // 주어진 방법으로 수정하면 True 반환   
        }
        else if (method == modificationMethods[2])
        {
            modificationSuccess = moveAndWait(modifedMatrix, nModification);     // 주어진 방법으로 수정하면 True 반환
        }
        else if (method == modificationMethods[3])
        {
            modificationSuccess = switchHands(modifedMatrix, nModification);     // 주어진 방법으로 수정하면 True 반환
        }
        else if (method == modificationMethods[4])
        {
            modificationSuccess = deleteInst(modifedMatrix, nModification);     // 주어진 방법으로 수정하면 True 반환
        }
        else
        {
            modificationSuccess = false;
        }

        // std::cout << "\n ////////////// modify Measure : " << method << "\n";
        // std::cout << modifedMatrix;
        // std::cout << "\n ////////////// \n";

        if (!detectCollision(modifedMatrix))   // 충돌 예측
        {
            // std::cout << "\n 성공 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n";
            
            measureMatrix = modifedMatrix;
            return true;
        }
        else
        {
            // std::cout << "\n 실패 ㅜㅜ \n";
            modifedMatrix = measureMatrix;
        }

        nModification++;
    }
    return false;
}

pair<int, int> PathManager::findModificationRange(VectorXd t, VectorXd instR, VectorXd instL)
{
    // 수정하면 안되는 부분 제외
    // detectLine 부터 수정 가능
    double hitDetectionThreshold = 1.2 * 100.0 / bpmOfScore; // 일단 이렇게 하면 1줄만 읽는 일 없음

    int detectLineR = 1;
    for (int i = 1; i < t.rows(); i++)
    {
        if (round(10000 * hitDetectionThreshold) < round(10000 * (t(i) - t(0))))
        {
            break;
        }

        if (instR(i) != 0)
        {
            detectLineR = i + 1;
            break;
        }
    }

    int detectLineL = 1;
    for (int i = 1; i < t.rows(); i++)
    {
        if (round(10000 * hitDetectionThreshold) < round(10000 * (t(i) - t(0))))
        {
            break;
        }

        if (instL(i) != 0)
        {
            detectLineL = i + 1;
            break;
        }
    }

    return make_pair(detectLineR, detectLineL);
}

bool PathManager::modifyCrash(MatrixXd &measureMatrix, int num)
{
    // 주어진 방법으로 수정하면 True 반환
    VectorXd t = measureMatrix.col(8);
    VectorXd instR = measureMatrix.col(2);
    VectorXd instL = measureMatrix.col(3);

    // 수정하면 안되는 부분 제외
    pair<int, int> detectLine = findModificationRange(t, instR, instL);

    int detectLineR = detectLine.first;
    int detectLineL = detectLine.second;

    // Modify Crash
    int cnt = 0;

    for (int i = detectLineR; i < t.rows(); i++)
    {
        if (instR(i) == 7)
        {
            if (cnt == num)
            {
                measureMatrix(i, 2) = 8;
                return true;
            }
            cnt++;
        }
        else if (instR(i) == 8)
        {
            if (cnt == num)
            {
                measureMatrix(i, 2) = 7;
                return true;
            }
            cnt++;
        }
    }

    for (int i = detectLineL; i < t.rows(); i++)
    {
        if (instL(i) == 7)
        {
            if (cnt == num)
            {
                measureMatrix(i, 3) = 8;
                return true;
            }
            cnt++;
        }
        else if (instL(i) == 8)
        {
            if (cnt == num)
            {
                measureMatrix(i, 3) = 7;
                return true;
            }
            cnt++;
        }
    }

    return false;
}

bool PathManager::switchHands(MatrixXd &measureMatrix, int num)
{
    // 주어진 방법으로 수정하면 True 반환
    VectorXd t = measureMatrix.col(8);
    VectorXd instR = measureMatrix.col(2);
    VectorXd instL = measureMatrix.col(3);

    // 수정하면 안되는 부분 제외
    pair<int, int> detectLine = findModificationRange(t, instR, instL);

    // 뒤쪽 목표위치 없는 부분 수정하면 안됨
    int endIndex = findDetectionRange(measureMatrix);

    int detectLineR = detectLine.first;
    int detectLineL = detectLine.second;

    // Modify Arm
    int cnt = 0;
    int maxDetectLine = 1;

    if (detectLineR > detectLineL)
    {
        maxDetectLine = detectLineR;
    }
    else
    {
        maxDetectLine = detectLineL;
    }

    for (int i = maxDetectLine; i < t.rows(); i++)
    {
        if ((instR(i) != 0) && (instL(i) != 0))
        {
            if (cnt == num)
            {
                int tmp = measureMatrix(i, 2);
                measureMatrix(i, 2) = measureMatrix(i, 3);
                measureMatrix(i, 3) = tmp;
                
                tmp = measureMatrix(i, 4);
                measureMatrix(i, 4) = measureMatrix(i, 5);
                measureMatrix(i, 5) = tmp;
                
                return true;
            }
            cnt++;
        }
    }

    for (int i = detectLineR; i < endIndex; i++)
    {
        if (instR(i) != 0)
        {
            if (instL(i) == 0)
            {
                if (cnt == num)
                {
                    measureMatrix(i, 3) = measureMatrix(i, 2);
                    measureMatrix(i, 5) = measureMatrix(i, 4);

                    measureMatrix(i, 2) = 0;
                    measureMatrix(i, 4) = 0;
                    return true;
                }
                cnt++;
            }
        }
    }

    for (int i = detectLineL; i < endIndex; i++)
    {
        if (instL(i) != 0)
        {
            if (instR(i) == 0)
            {
                if (cnt == num)
                {
                    measureMatrix(i, 2) = measureMatrix(i, 3);
                    measureMatrix(i, 4) = measureMatrix(i, 5);

                    measureMatrix(i, 3) = 0;
                    measureMatrix(i, 5) = 0;
                    return true;
                }
                cnt++;
            }
        }
    }

    return false;
}

bool PathManager::waitAndMove(MatrixXd &measureMatrix, int num)
{
    // 주어진 방법으로 수정하면 True 반환
    VectorXd t = measureMatrix.col(8);
    VectorXd instR = measureMatrix.col(2);
    VectorXd instL = measureMatrix.col(3);

    // 수정하면 안되는 부분 제외
    pair<int, int> detectLine = findModificationRange(t, instR, instL);

    int detectLineR = detectLine.first;
    int detectLineL = detectLine.second;

    // Wait and Move
    int cnt = 0;

    bool isStart = false;
    int startInst, startIndex;
    for (int i = detectLineL-1; i < t.rows(); i++)
    {
        if (instL(i) != 0)
        {
            if (isStart)
            {
                if (startInst != instL(i))
                {
                    for (int j = 1; j < i-startIndex; j++)
                    {
                        if (cnt == num)
                        {
                            measureMatrix(startIndex+j, 3) = startInst;
                            return true;
                        }
                        cnt++;
                    }
                }
                
                startIndex = i;
                startInst = instL(i);
            }
            else
            {
                isStart = true;
                startIndex = i;
                startInst = instL(i);
            }
        }
    }

    isStart = false;
    for (int i = detectLineR-1; i < t.rows(); i++)
    {
        if (instR(i) != 0)
        {
            if (isStart)
            {
                if (startInst != instR(i))
                {
                    for (int j = 1; j < i-startIndex; j++)
                    {
                        if (cnt == num)
                        {
                            measureMatrix(startIndex+j, 2) = startInst;
                            return true;
                        }
                        cnt++;
                    }
                }

                startIndex = i;
                startInst = instR(i);
            }
            else
            {
                isStart = true;
                startIndex = i;
                startInst = instR(i);
            }
        }
    }

    return false;
}

bool PathManager::moveAndWait(MatrixXd &measureMatrix, int num)
{
    // 주어진 방법으로 수정하면 True 반환
    VectorXd t = measureMatrix.col(8);
    VectorXd instR = measureMatrix.col(2);
    VectorXd instL = measureMatrix.col(3);

    // 수정하면 안되는 부분 제외
    pair<int, int> detectLine = findModificationRange(t, instR, instL);

    int detectLineR = detectLine.first;
    int detectLineL = detectLine.second;

    // Move and Wait
    int cnt = 0;

    bool isStart = false;
    int startInst, endInst, startIndex;
    for (int i = detectLineL-1; i < t.rows(); i++)
    {
        if (instL(i) != 0)
        {
            if (isStart)
            {
                endInst = instL(i);
                if (endInst != startInst)
                {
                    for (int j = 1; j < i-startIndex; j++)
                    {
                        if (cnt == num)
                        {
                            measureMatrix(i-j, 3) = endInst;
                            return true;
                        }
                        cnt++;
                    }
                }
                
                startIndex = i;
                startInst = instL(i);
            }
            else
            {
                isStart = true;
                startIndex = i;
                startInst = instL(i);
            }
        }
    }

    isStart = false;
    for (int i = detectLineR-1; i < t.rows(); i++)
    {
        if (instR(i) != 0)
        {
            if (isStart)
            {
                endInst = instR(i);
                if (endInst != startInst)
                {
                    for (int j = 1; j < i-startIndex; j++)
                    {
                        if (cnt == num)
                        {
                            measureMatrix(i-j, 2) = endInst;
                            return true;
                        }
                        cnt++;
                    }
                }
                
                startIndex = i;
                startInst = instR(i);
            }
            else
            {
                isStart = true;
                startIndex = i;
                startInst = instR(i);
            }
        }
    }

    return false;
}

bool PathManager::deleteInst(MatrixXd &measureMatrix, int num)
{
    // 주어진 방법으로 수정하면 True 반환
    VectorXd t = measureMatrix.col(8);
    VectorXd instR = measureMatrix.col(2);
    VectorXd instL = measureMatrix.col(3);

    // 수정하면 안되는 부분 제외
    pair<int, int> detectLine = findModificationRange(t, instR, instL);

    // 뒤쪽 목표위치 없는 부분 수정하면 안됨
    int endIndex = findDetectionRange(measureMatrix);

    int detectLineR = detectLine.first;
    int detectLineL = detectLine.second;

    // Move and Wait
    int cnt = 0;

    for (int i = detectLineR; i < endIndex; i++)
    {
        if (instR(i) != 0)
        {
            if (cnt == num)
            {
                measureMatrix(i, 2) = 0;
                measureMatrix(i, 4) = 0;
                return true;
            }
            cnt++;
        }
    }

    for (int i = detectLineL; i < endIndex; i++)
    {
        if (instL(i) != 0)
        {
            if (cnt == num)
            {
                measureMatrix(i, 3) = 0;
                measureMatrix(i, 5) = 0;
                return true;
            }
            cnt++;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
/*                                   IK                                       */
////////////////////////////////////////////////////////////////////////////////

VectorXd PathManager::solveGeometricIK(VectorXd &pR, VectorXd &pL, double theta0, double theta7, double theta8, bool printError)
{
    VectorXd output;
    PartLength partLength;

    double XR = pR(0), YR = pR(1), ZR = pR(2);
    double XL = pL(0), YL = pL(1), ZL = pL(2);
    double R1 = partLength.upperArm;
    double R2 = getLength(theta7);
    double L1 = partLength.upperArm;
    double L2 = getLength(theta8);
    double s = partLength.waist;
    double z0 = partLength.height;

    double shoulderXR = 0.5 * s * cos(theta0);
    double shoulderYR = 0.5 * s * sin(theta0);
    double shoulderXL = -0.5 * s * cos(theta0);
    double shoulderYL = -0.5 * s * sin(theta0);

    double theta01 = atan2(YR - shoulderYR, XR - shoulderXR);
    double theta1 = theta01 - theta0;

    double err = 0.0;  // IK 풀림

    if (theta1 < 0 || theta1 > 150.0 * M_PI / 180.0) // the1 범위 : 0deg ~ 150deg
    {
        if (printError)
        {
            // std::cout << "PR: " << pR.transpose() << "\n";
            // std::cout << "PL: " << pL.transpose() << "\n";
            // std::cout << "theta0: " << theta0 * 180.0 / M_PI << ", theta7: " << theta7 * 180.0 / M_PI << ", theta8: " << theta8 * 180.0 / M_PI << "\n";
            std::cout << "IKFUN (q1) is not solved!!\n";
            std::cout << "theta1: " << theta1 * 180.0 / M_PI << "\n";
        }
        err = 1.0;  // IK 안풀림
    }

    double theta02 = atan2(YL - shoulderYL, XL - shoulderXL);
    double theta2 = theta02 - theta0;

    if (theta2 < 30 * M_PI / 180.0 || theta2 > M_PI) // the2 범위 : 30deg ~ 180deg
    {
        if (printError)
        {
            // std::cout << "PR: " << pR.transpose() << "\n";
            // std::cout << "PL: " << pL.transpose() << "\n";
            // std::cout << "theta0: " << theta0 * 180.0 / M_PI << ", theta7: " << theta7 * 180.0 / M_PI << ", theta8: " << theta8 * 180.0 / M_PI << "\n";
            std::cout << "IKFUN (q2) is not solved!!\n";
            std::cout << "theta2: " << theta2 * 180.0 / M_PI << "\n";
        }
        err = 1.0;  // IK 안풀림
    }

    double zeta = z0 - ZR;
    double r2 = (YR - shoulderYR) * (YR - shoulderYR) + (XR - shoulderXR) * (XR - shoulderXR); // r^2

    double x = zeta * zeta + r2 - R1 * R1 - R2 * R2;
    double y = sqrt(4.0 * R1 * R1 * R2 * R2 - x * x);

    if (4.0 * L1 * L1 * L2 * L2 - x * x < 0)
    {
        err = 1.0;  // IK 안풀림

        output.resize(10);
        output << 99.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, err;    // theta 0을 90도로 설정 -> Error State로 이동

        if (printError)
        {
            std::cout << "IKFUN is not solved!! (sqrt)\n";
        }

        return output;
    }
    
    double theta4 = atan2(y, x);
    double theta34 = atan2(sqrt(r2), zeta);
    double theta3 = theta34 - atan2(R2 * sin(theta4), R1 + R2 * cos(theta4));

    if (theta3 < -45.0 * M_PI / 180.0 || theta3 > 90 * M_PI / 180.0) // the3 범위 : -45deg ~ 90deg
    {
        if (printError)
        {
            // std::cout << "PR: " << pR.transpose() << "\n";
            // std::cout << "PL: " << pL.transpose() << "\n";
            // std::cout << "theta0: " << theta0 * 180.0 / M_PI << ", theta7: " << theta7 * 180.0 / M_PI << ", theta8: " << theta8 * 180.0 / M_PI << "\n";
            std::cout << "IKFUN (q3) is not solved!!\n";
            std::cout << "theta3: " << theta3 * 180.0 / M_PI << "\n";
        }
        err = 1.0;  // IK 안풀림
    }

    zeta = z0 - ZL;
    r2 = (YL - shoulderYL) * (YL - shoulderYL) + (XL - shoulderXL) * (XL - shoulderXL); // r^2

    x = zeta * zeta + r2 - L1 * L1 - L2 * L2;
    y = sqrt(4.0 * L1 * L1 * L2 * L2 - x * x);

    if (4.0 * L1 * L1 * L2 * L2 - x * x < 0)
    {
        err = 1.0;  // IK 안풀림
        
        output.resize(10);
        output << 99.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, err;    // theta 0을 90도로 설정 -> Error State로 이동

        if (printError)
        {
            std::cout << "IKFUN is not solved!! (sqrt)\n";
        }

        return output;
    }

    double theta6 = atan2(y, x);
    double theta56 = atan2(sqrt(r2), zeta);
    double theta5 = theta56 - atan2(L2 * sin(theta6), L1 + L2 * cos(theta6));

    if (theta5 < -45.0 * M_PI / 180.0 || theta5 > 90 * M_PI / 180.0) // the5 범위 : -45deg ~ 90deg
    {
        if (printError)
        {
            // std::cout << "PR: " << pR.transpose() << "\n";
            // std::cout << "PL: " << pL.transpose() << "\n";
            // std::cout << "theta0: " << theta0 * 180.0 / M_PI << ", theta7: " << theta7 * 180.0 / M_PI << ", theta8: " << theta8 * 180.0 / M_PI << "\n";
            std::cout << "IKFUN (q5) is not solved!!\n";
            std::cout << "theta5: " << theta5 * 180.0 / M_PI << "\n";
        }
        err = 1.0;  // IK 안풀림
    }

    theta4 -= getTheta(R2, theta7);
    theta6 -= getTheta(L2, theta8);

    if (theta4 < 0 || theta4 > 140.0 * M_PI / 180.0) // the4 범위 : 0deg ~ 120deg
    {
        if (printError)
        {
            // std::cout << "PR: " << pR.transpose() << "\n";
            // std::cout << "PL: " << pL.transpose() << "\n";
            // std::cout << "theta0: " << theta0 * 180.0 / M_PI << ", theta7: " << theta7 * 180.0 / M_PI << ", theta8: " << theta8 * 180.0 / M_PI << "\n";
            std::cout << "IKFUN (q4) is not solved!!\n";
            std::cout << "theta4: " << theta4 * 180.0 / M_PI << "\n";
        }
        err = 1.0;  // IK 안풀림
    }

    if (theta6 < 0 || theta6 > 140.0 * M_PI / 180.0) // the6 범위 : 0deg ~ 120deg
    {
        if (printError)
        {
            // std::cout << "PR: " << pR.transpose() << "\n";
            // std::cout << "PL: " << pL.transpose() << "\n";
            // std::cout << "theta0: " << theta0 * 180.0 / M_PI << ", theta7: " << theta7 * 180.0 / M_PI << ", theta8: " << theta8 * 180.0 / M_PI << "\n";
            std::cout << "IKFUN (q6) is not solved!!\n";
            std::cout << "theta6: " << theta6 * 180.0 / M_PI << "\n";
        }
        err = 1.0;  // IK 안풀림
    }

    output.resize(10);
    output << theta0, theta1, theta2, theta3, theta4, theta5, theta6, theta7, theta8, err;

    for (int i = 0; i < 9; i++)
    {
        if (std::isnan(output(i)))
        {
            output(9) = 1.0;   // IK 안풀림

            if (printError)
            {
                std::cout << "IKFUN is not solved!! (nan)\n";
            }

            break;
        }
    }

    return output;
}

double PathManager::getLength(double theta)
{
    PartLength partLength;
    double l1 = partLength.lowerArm;
    double l2 = partLength.stick;

    double l3 = l1 + l2 * cos(theta);

    double l4 = sqrt(l3 * l3 + ((l2 * sin(theta)) * (l2 * sin(theta))));

    return l4;
}

double PathManager::getTheta(double l1, double theta)
{
    PartLength partLength;

    double l2 = partLength.lowerArm + partLength.stick * cos(theta);

    double theta_m = acos(l2 / l1);

    return theta_m;
}
