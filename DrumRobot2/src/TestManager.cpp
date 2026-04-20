// TestManager.cpp
#include "../include/managers/TestManager.hpp"
#include "../3rdparty/aruco/include/opencv2/aruco.hpp"
// For  Qt

using namespace std;
using namespace cv;

TestManager::TestManager(State &stateRef, CanManager &canManagerRef, std::map<std::string, std::shared_ptr<GenericMotor>> &motorsRef, USBIO &usbioRef, Functions &funRef)
    : state(stateRef), canManager(canManagerRef), motors(motorsRef), usbio(usbioRef), func(funRef)
{
    standardTime = chrono::system_clock::now();
}

void TestManager::SendTestProcess()
{
    while(1)
    {    
        int ret = system("clear");
        if (ret == -1) std::cout << "system clear error" << endl;

        float c_MotorAngle[12];
        getMotorPos(c_MotorAngle);

        std::cout << "[ Current Q Values (Radian / Degree) ]\n";
        for (int i = 0; i < 12; i++)
        {
            q[i] = c_MotorAngle[i];
            std::cout << "Q[" << i << "] : " << c_MotorAngle[i] << "\t\t" << c_MotorAngle[i] * 180.0 / M_PI << "\n";
            }
        FK(c_MotorAngle); // 현재 q값에 대한 FK 진행
    
        std::cout << "\nSelect Method (1 - 관절각도값 조절, 2 - 좌표값 조절, 4 - 발 모터, 5 - 속도 제어 실험, 6 - 브레이크, 7 - 허리 모터, -1 - 나가기) : ";
        std::cin >> method;

        if(method == 1)
        {
            while(1)
            {
                int userInput = 100;
                int ret = system("clear");
                if (ret == -1) std::cout << "system clear error" << endl;

                float c_MotorAngle[10] = {0};
                getMotorPos(c_MotorAngle);

                std::cout << "[ Current Q Values (Radian / Degree) ]\n";
                for (int i = 0; i < 10; i++)
                {
                    std::cout << "Q[" << i << "] : " << c_MotorAngle[i] << "\t\t" << c_MotorAngle[i] * 180.0 / M_PI << "\t\t" <<q[i]/ M_PI * 180.0 << "\n";
                    }
                FK(c_MotorAngle);

                std::cout << "\ntime : " << t << "s";
                std::cout << "\nnumber of repeat : " << n_repeat << std::endl << std::endl;


                std::cout << "\nSelect Motor to Change Value (0-8) / Run (9) / Time (10) / Extra Time (11) / Repeat(12) / break on off (13) / Exit (-1): ";
                std::cin >> userInput;

                if (userInput == -1)
                {
                    break;
                }
                else if (userInput < 9)
                {
                    float degree_angle;

                    std::cout << "\nRange : " << jointRangeMin[userInput] << "~" << jointRangeMax[userInput] << "(Degree)\n";
                    std::cout << "Enter q[" << userInput << "] Values (Degree) : ";
                    std::cin >> degree_angle;
                    q[userInput] = degree_angle * M_PI / 180.0;
                }
                else if (userInput == 9)
                {
                    for (auto &motor_pair : motors)
                    {
                        if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(motor_pair.second))
                        {
                            tMotor->clearCommandBuffer();
                            tMotor->clearReceiveBuffer();
                        }
                        else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(motor_pair.second))
                        {
                            maxonMotor->clearCommandBuffer();
                            maxonMotor->clearReceiveBuffer();
                        }
                    }
                    getArr(q);
                }
                else if (userInput == 10)
                {
                    std::cout << "time : ";
                    std::cin >> t;
                }
                else if (userInput == 11)
                {
                    std::cout << "extra time : ";
                    std::cin >> extra_time;
                }
                else if (userInput == 12)
                {
                    std::cout << "number of repeat : ";
                    std::cin >> n_repeat;
                }
            }
        }
        else if (method == 2)
        {
            while(1)
            {
                int userInput = 100;
                int ret = system("clear");
                if (ret == -1)
                    std::cout << "system clear error" << endl;
                std::cout << "[ Current x, y, z (meter) ]\n";
                std::cout << "Right : ";
                for (int i = 0; i < 3; i++)
                {
                    std::cout << R_xyz[i] << ", ";
                }
                std::cout << "\nLeft : ";
                for (int i = 0; i < 3; i++)
                {
                    std::cout << L_xyz[i] << ", ";
                }

                std::cout << "\nSelect Motor to Change Value (1 - Right, 2 - Left) / Start Test (3) / Exit (-1) : ";
                std::cin >> userInput;

                if (userInput == -1)
                {
                    break;
                }
                else if (userInput == 1)
                {
                    std::cout << "Enter x, y, z Values (meter) : ";
                    std::cin >> R_xyz[0] >> R_xyz[1] >> R_xyz[2];
                }
                else if (userInput == 2)
                {
                    std::cout << "Enter x, y, z Values (meter) : ";
                    std::cin >> L_xyz[0] >> L_xyz[1] >> L_xyz[2];
                }
                else if (userInput == 3)
                {
                    VectorXd pR(3), pL(3);
                    for (int i = 0; i < 3; i++)
                    {
                        pR(i) = R_xyz[i];
                        pL(i) = L_xyz[i];
                    }
                    
                    VectorXd waistVector = calWaistAngle(pR, pL);
                    VectorXd qk = IKFixedWaist(pR, pL, 0.5 * (waistVector(0) + waistVector(1)));

                    int qn = qk.size();
                    for (int i = 0; i < qn && i < 10; ++i) {
                        q[i] = qk(i);
                    }
                    getArr(q);
                }
            }
           
        }               
        else if (method == 4)
        {
            while(1)
            {
                int userInput = 100;
                int ret = system("clear");
                if (ret == -1) std::cout << "system clear error" << endl;

                float c_MotorAngle[12] = {0};
                getMotorPos(c_MotorAngle);

                std::cout << "[ Current Q Values (Radian / Degree) ]\n";
                for (int i = 10; i < 12; i++)
                {
                    std::cout << "Q[" << i << "] : " << c_MotorAngle[i] << "\t\t" << c_MotorAngle[i] * 180.0 / M_PI << "\t\t" << q[i]/ M_PI * 180.0 << "\n";
                }

                std::cout << "\ntime : " << t << "s";
                std::cout << "\nextra time : " << extra_time << "s";
                std::cout << "\nnumber of repeat : " << n_repeat << std::endl << std::endl;


                std::cout << "\nSelect Motor to Change Value (R: 10, L: 11) / Run (1) / Time (2) / Extra Time (3) / Repeat (4) / sine traj (5) / Exit (-1): ";
                std::cin >> userInput;

                if (userInput == -1)
                {
                    break;
                }
                else if (userInput == 10 || userInput == 11)
                {
                    float degree_angle;

                    std::cout << "\nRange : " << jointRangeMin[userInput] << "~" << jointRangeMax[userInput] << "(Degree)\n";
                    std::cout << "Enter q[" << userInput << "] Values (Degree) : ";
                    std::cin >> degree_angle;
                    q[userInput] = degree_angle * M_PI / 180.0;
                }
                else if (userInput == 1)
                {
                    for (auto &motor_pair : motors)
                    {
                        if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(motor_pair.second))
                        {
                            tMotor->clearCommandBuffer();
                            tMotor->clearReceiveBuffer();
                        }
                        else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(motor_pair.second))
                        {
                            maxonMotor->clearCommandBuffer();
                            maxonMotor->clearReceiveBuffer();
                        }
                    }
                    getArr(q);
                }
                else if (userInput == 2)
                {
                    std::cout << "time : ";
                    std::cin >> t;
                }
                else if (userInput == 3)
                {
                    std::cout << "extra time : ";
                    std::cin >> extra_time;
                }
                else if (userInput == 4)
                {
                    std::cout << "number of repeat : ";
                    std::cin >> n_repeat;
                }
                else if (userInput == 5)
                {
                    float A = 90.0;
                    
                    std::cout << "A : ";
                    std::cin >> A;
                    
                    float t_now = 0.0;
                    float dt = 0.005;
                    
                    while(t_now<=t)
                    {   
                        vector<float> Qi(12);

                        for (int i = 0; i < 12; i++)
                        {
                            Qi[i] = A * sin (2 * M_PI * t_now / t) * M_PI / 180.0 + c_MotorAngle[i];
                            //Qi[i] = A * (1 - cos(2 * M_PI * t_now / t)) / 2.0 * M_PI / 180.0 + c_MotorAngle[i];
                        }
                        // Send to Buffer
                        for (auto &entry : motors)
                        {
                            if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
                            {
                                TMotorData newData;
                                newData.position = tMotor->jointAngleToMotorPosition(Qi[canManager.motorMapping[entry.first]]);
                                newData.mode = tMotor->Position;
                                {
                                    std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
                                    tMotor->commandBuffer.push(newData);
                                }
                                
                                tMotor->finalMotorPosition = newData.position;
                            }
                            else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
                            {
                                MaxonData newData;
                                newData.position = maxonMotor->jointAngleToMotorPosition(Qi[canManager.motorMapping[entry.first]]);
                                newData.mode = maxonMotor->CSP;
                                {
                                    std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);
                                    maxonMotor->commandBuffer.push(newData);
                                }

                                maxonMotor->finalMotorPosition = newData.position;
                            }
                        }
                        //std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        t_now += dt;
                    }
                
                }
            
            }
        }
        else if(method == 5)
        {
            testTmotorVelocityMode();
        }
        else if(method == 6)
        {
            int state_brake;
            cin >> state_brake;
            //useBrake가 1이면 브레이크 켜줌 0이면 꺼줌
            usbio.setUSBIO4761(0, state_brake); //세팅
            usbio.outputUSBIO4761();                    //실행
        }
        else if(method == 7)
        {
            float t_now = 0.0f;
            float dt = 0.005f;

            float target_deg;
            float move_time;
            int wait_time = move_time;
            
            float c_MotorAngle[12] = {0};
            getMotorPos(c_MotorAngle);
            
            float start_rad = c_MotorAngle[0];

            std::cout << "========================================" << std::endl;
            std::cout << " [Waist Moving mode] " << std::endl;
            std::cout << " Current Waist Angle: " << start_rad * 180.0 / M_PI << " [deg]" << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            std::cout << " Enter Goal Angle (deg): ";
            std::cin >> target_deg;
            std::cout << " Enter Moving Time (sec): ";
            std::cin >> move_time;

            // 예외 처리: 시간이 0 이하이면 실행 방지
            if (move_time <= 0) {
                std::cout << " [Error] Time must be > 0." << std::endl;
                return; 
            }

            float target_rad = target_deg * M_PI / 180.0f;

            std::cout << " Moving Start..." << std::endl;

            // 4. 제어 루프 시작
            while(t_now <= move_time)
            {   
                float Q = ((target_rad - start_rad) / 2.0f) * cos(M_PI * (t_now / move_time + 1.0f)) + ((target_rad + start_rad) / 2.0f);

                for (auto &entry : motors)
                {
                    if (entry.first == "waist") 
                    {
                        if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
                        {
                            TMotorData newData;
                            
                            newData.position = tMotor->jointAngleToMotorPosition(Q);
                            newData.mode = tMotor->Position; 
                            {
                                std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
                                tMotor->commandBuffer.push(newData);
                            }
                            tMotor->finalMotorPosition = newData.position;
                        }
                    }
                }
                t_now += dt;
            }
            for(int i = 0; i<wait_time; i++)
            {
                std::cout << i << "\t";
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout << " Moving Complete." << std::endl;

            camera_calibration(target_deg);

            std::cout << " Scanning Complete." << std::endl;
        }
        else
        {
            break;
        }
    }
}

void TestManager::FK(float theta[])
{
    vector<float> P;

    PartLength partLength;

    float r1 = partLength.upperArm;
    float r2 = partLength.lowerArm;
    float l1 = partLength.upperArm;
    float l2 = partLength.lowerArm;
    float stick = partLength.stick;
    float s = partLength.waist;
    float z0 = partLength.height;

    P.push_back(0.5 * s * cos(theta[0]) + r1 * sin(theta[3]) * cos(theta[0] + theta[1]) + r2 * sin(theta[3] + theta[4]) * cos(theta[0] + theta[1]) + stick * sin(theta[3] + theta[4] + theta[7]) * cos(theta[0] + theta[1]));
    P.push_back(0.5 * s * sin(theta[0]) + r1 * sin(theta[3]) * sin(theta[0] + theta[1]) + r2 * sin(theta[3] + theta[4]) * sin(theta[0] + theta[1]) + stick * sin(theta[3] + theta[4] + theta[7]) * sin(theta[0] + theta[1]));
    P.push_back(z0 - r1 * cos(theta[3]) - r2 * cos(theta[3] + theta[4]) - stick * cos(theta[3] + theta[4] + theta[7]));
    P.push_back(-0.5 * s * cos(theta[0]) + l1 * sin(theta[5]) * cos(theta[0] + theta[2]) + l2 * sin(theta[5] + theta[6]) * cos(theta[0] + theta[2]) + stick * sin(theta[5] + theta[6] + theta[8]) * cos(theta[0] + theta[2]));
    P.push_back(-0.5 * s * sin(theta[0]) + l1 * sin(theta[5]) * sin(theta[0] + theta[2]) + l2 * sin(theta[5] + theta[6]) * sin(theta[0] + theta[2]) + stick * sin(theta[5] + theta[6] + theta[8]) * sin(theta[0] + theta[2]));
    P.push_back(z0 - l1 * cos(theta[5]) - l2 * cos(theta[5] + theta[6]) - stick * cos(theta[5] + theta[6] + theta[8]));

    std::cout << "\nRight Hand Position : { " << P[0] << " , " << P[1] << " , " << P[2] << " }\n";
    std::cout << "Left Hand Position : { " << P[3] << " , " << P[4] << " , " << P[5] << " }\n";
}

/////////////////////////////////////////////////////////////////////////////////
/*                                 Values Test Mode                           */
/////////////////////////////////////////////////////////////////////////////////

void TestManager::getMotorPos(float c_MotorAngle[])
{
    // 각 모터의 현재위치 값 불러오기 ** CheckMotorPosition 이후에 해야함(변수값을 불러오기만 해서 갱신 필요)
    for (auto &entry : motors)
    {
        if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
        {
            c_MotorAngle[canManager.motorMapping[entry.first]] = tMotor->jointAngle;
        }
        if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
        {
            c_MotorAngle[canManager.motorMapping[entry.first]] = maxonMotor->jointAngle;
        }
    }
}

vector<float> TestManager::cal_Vmax(float q1[], float q2[],  float acc, float t2)
{
    vector<float> Vmax;

    for (long unsigned int i = 0; i < 12; i++)
    {
        float val;
        float S = q2[i] - q1[i];

        // 이동거리 양수로 변경
        if (S < 0)
        {
            S = -1 * S;
        }

        if (S > t2*t2*acc/4)
        {
            // 가속도로 도달 불가능
            // -1 반환
            val = -1;
        }
        else
        {
            // 2차 방정식 계수
            float A = 1/acc;
            float B = -1*t2;
            float C = S;
            float sol1 = (-B+sqrt(B*B-4*A*C))/2/A;
            float sol2 = (-B-sqrt(B*B-4*A*C))/2/A;
            if (sol1 >= 0 && sol1 <= acc*t2/2)
            {
                val = sol1;
            }
            else if (sol2 >= 0 && sol2 <= acc*t2/2)
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
        Vmax.push_back(val);
        cout << "Vmax_" << i << " : " << val << "rad/s\n";
    }

    return Vmax;
}

vector<float> TestManager::makeProfile(float q1[], float q2[], vector<float> &Vmax, float acc, float t, float t2)
{
    vector<float> Qi;
    for(long unsigned int i = 0; i < 12; i++)
    {
        float val, S;
        int sign;
        S = q2[i] - q1[i];   
        // 부호 확인
        if (S < 0)
        {
            S = -1 * S;
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
            val = q1[i];
        }
        else if (Vmax[i] < 0)
        {
            // Vmax 값을 구하지 못했을 때 삼각형 프로파일 생성
            float acc_tri = 4 * S / t2 / t2;
            if (t < t2/2)
            {
                val = q1[i] + sign * 0.5 * acc_tri * t * t;
            }
            else if (t < t2)
            {
                val = q2[i] - sign * 0.5 * acc_tri * (t2 - t) * (t2 - t);
            }
            else
            {
                val = q2[i];
            }
        }
        else
        {
            // 사다리꼴 프로파일
            if (t < Vmax[i] / acc)
            {
                // 가속
                val = q1[i] + sign * 0.5 * acc * t * t;
            }
            else if (t < S / Vmax[i])
            {
                // 등속
                val = q1[i] + (sign * 0.5 * Vmax[i] * Vmax[i] / acc) + (sign * Vmax[i] * (t - Vmax[i] / acc));          
            }
            else if (t < Vmax[i] / acc + S / Vmax[i])
            {
                // 감속
                val = q2[i] - sign * 0.5 * acc * (S / Vmax[i] + Vmax[i] / acc - t) * (S / Vmax[i] + Vmax[i] / acc - t);          
            }
            else 
            {
                val = q2[i];
            }
        }
        Qi.push_back(val);
    }
    return Qi;

    // vector<float> Qi;
    // float acceleration = 100; //320000 / 21 / 10 * 2 * M_PI / 60;  // rad/s^2 
    // int sign;
    // float Vmax = 0;
    // float S;
    // static int loop_count =0;

    // for (int i = 0; i < 9; i++)
    // {
    //     float val;

    //     S = Q2[i] - Q1[i];

    //     if (S < 0)
    //     {
    //         S = -1 * S;
    //         sign = -1;
    //     }
    //     else
    //     {
    //         sign = 1;
    //     }
    //     // 2차 방정식의 계수들
    //     float a = 1.0 / acceleration;
    //     float b = -n;
    //     float c = S;
    //     float discriminant = (b * b) - (4 * a * c);

    //     if (discriminant < 0)
    //     {
    //         // if(i ==4)
    //         // {
    //         // std::cout << "No real solution for Vmax." << std::endl;
    //         // sleep(1);
    //         // }
    //         val = -1;   //Qi.push_back(-1); // 실수 해가 없을 경우 -1 추가
    //         Qi.push_back(val);
    //         continue;
    //     }
    //     else
    //     {
    //         // 2차 방정식의 해 구하기
    //         float Vmax1 = (-b + std::sqrt(discriminant)) / (2 * a);
    //         float Vmax2 = (-b - std::sqrt(discriminant)) / (2 * a);
            
    //         // 두 해 중 양수인 해 선택
    //         if (Vmax1 > 0 && Vmax1 < 0.5*n*acceleration)
    //         {
    //             Vmax = Vmax1;

    //         }
    //         else if (Vmax2 > 0 && Vmax2 < 0.5*n*acceleration)
    //         {
    //             Vmax = Vmax2;

    //         }
    //         else
    //         {
    //             //std::cout << "No real solution for Vmax." << std::endl;
    //             Qi.push_back(Q1[i]); // 실수 해가 없을 경우
    //             continue;
    //         }
    //         //std::cout << "Calculated Vmax: " << Vmax << std::endl;
    //     }

    //     if (S == 0)
    //     {
    //         // 정지
    //         val = Q1[i];
    //     }
    //     else// if ((Vmax * Vmax / acceleration) < S)
    //     {
    //         // 가속
    //         if (k < Vmax / acceleration)
    //         {
    //             val = Q1[i] + sign * 0.5 * acceleration * k * k;
    //             // if(i==4)
    //             // {
    //             // std::cout <<"가속 : " <<val<< std::endl;
    //             // }
    //         }
    //         // 등속
    //         else if (k < S / Vmax)
    //         {
    //             val = Q1[i] + (sign * 0.5 * Vmax * Vmax / acceleration) + (sign * Vmax * (k - Vmax / acceleration)); 
    //         //    if(i==4)
    //         //     {
    //         //     std::cout <<"등속 : " <<val<< std::endl;
    //         //     }            
    //         }
    //         // 감속
    //         else if (k < Vmax / acceleration + S / Vmax)
    //         {
    //             val = Q2[i] - sign * 0.5 * acceleration * (S / Vmax + Vmax / acceleration - k) * (S / Vmax + Vmax / acceleration - k);
    //             // if(i ==4)
    //             // {
    //             // std::cout <<"감속 : " <<val<< std::endl;
    //             // }               
    //         }           
    //         else 
    //         {
    //             val = Q2[i];
    //             // if(i ==4)                
    //             // {
    //             // std::cout <<"else : " <<val<< std::endl;
    //             // }                   
    //         }
    //     }

    //     Qi.push_back(val);

    // }
    // loop_count ++;
    // // cout << " Qi[3] : "<< Qi[3] << " Qi[4] : "<< Qi[4] <<endl;
    // return Qi;
}

void TestManager::getArr(float arr[])
{
    const float acc_max = 100.0;    // rad/s^2
    vector<float> Qi;
    vector<float> Vmax;
    float Q1[12] = {0.0};
    float Q2[12] = {0.0};
    int n;
    int n_p;    // 목표위치까지 가기 위한 추가 시간

    n = (int)(t/canManager.DTSECOND);    // t초동안 이동
    n_p = (int)(extra_time/canManager.DTSECOND);  // 추가 시간
    
    std::cout << "Get Array...\n";

    getMotorPos(Q1);

    for (int i = 0; i < 12; i++)
    {
        Q2[i] = arr[i];
    }

    Vmax = cal_Vmax(Q1, Q2, acc_max, t);
    
    for (int i = 0; i < n_repeat; i++)
    {
        for (int k = 1; k <= n + n_p; ++k)
        {
            // Make Vector
            if ((i%2) == 0)
            {
                Qi = makeProfile(Q1, Q2, Vmax, acc_max, t*k/n, t);
            }
            else
            {
                Qi = makeProfile(Q2, Q1, Vmax, acc_max, t*k/n, t);
            }

            // Send to Buffer
            for (auto &entry : motors)
            {
                if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
                {
                    TMotorData newData;
                    newData.position = tMotor->jointAngleToMotorPosition(Qi[canManager.motorMapping[entry.first]]);
                    newData.mode = tMotor->Position;
                    {
                        std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
                        tMotor->commandBuffer.push(newData);
                    }
                    
                    tMotor->finalMotorPosition = newData.position;
                }
                else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
                {
                    MaxonData newData;
                    newData.position = maxonMotor->jointAngleToMotorPosition(Qi[canManager.motorMapping[entry.first]]);
                    newData.mode = maxonMotor->CSP;
                    {
                        std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);
                        maxonMotor->commandBuffer.push(newData);
                    }

                    maxonMotor->finalMotorPosition = newData.position;
                }
            }
        }
    }
}

VectorXd TestManager::IKFixedWaist(VectorXd pR, VectorXd pL, double theta0)
{
    VectorXd Qf;
    PartLength partLength;

    float XR = pR(0), YR = pR(1), ZR = pR(2);
    float XL = pL(0), YL = pL(1), ZL = pL(2);
    float R1 = partLength.upperArm;
    float R2 = partLength.lowerArm + partLength.stick;
    float L1 = partLength.upperArm;
    float L2 = partLength.lowerArm + partLength.stick;
    float s = partLength.waist;
    float z0 = partLength.height;

    float shoulderXR = 0.5 * s * cos(theta0);
    float shoulderYR = 0.5 * s * sin(theta0);
    float shoulderXL = -0.5 * s * cos(theta0);
    float shoulderYL = -0.5 * s * sin(theta0);

    float theta01 = atan2(YR - shoulderYR, XR - shoulderXR);
    float theta1 = theta01 - theta0;

    if (theta1 < 0 || theta1 > 150.0 * M_PI / 180.0) // the1 범위 : 0deg ~ 150deg
    {
        std::cout << "IKFUN (q1) is not solved!!\n";
        state.main = Main::Error;
    }

    float theta02 = atan2(YL - shoulderYL, XL - shoulderXL);
    float theta2 = theta02 - theta0;

    if (theta2 < 30 * M_PI / 180.0 || theta2 > M_PI) // the2 범위 : 30deg ~ 180deg
    {
        std::cout << "IKFUN (q2) is not solved!!\n";
        state.main = Main::Error;
    }

    float zeta = z0 - ZR;
    float r2 = (YR - shoulderYR) * (YR - shoulderYR) + (XR - shoulderXR) * (XR - shoulderXR); // r^2

    float x = zeta * zeta + r2 - R1 * R1 - R2 * R2;
    float y = sqrt(4.0 * R1 * R1 * R2 * R2 - x * x);

    float theta4 = atan2(y, x);

    if (theta4 < 0 || theta4 > 140.0 * M_PI / 180.0) // the4 범위 : 0deg ~ 120deg
    {
        std::cout << "IKFUN (q4) is not solved!!\n";
        state.main = Main::Error;
    }

    float theta34 = atan2(sqrt(r2), zeta);
    float theta3 = theta34 - atan2(R2 * sin(theta4), R1 + R2 * cos(theta4));

    if (theta3 < -45.0 * M_PI / 180.0 || theta3 > 90.0 * M_PI / 180.0) // the3 범위 : -45deg ~ 90deg
    {
        std::cout << "IKFUN (q3) is not solved!!\n";
        state.main = Main::Error;
    }

    zeta = z0 - ZL;
    r2 = (YL - shoulderYL) * (YL - shoulderYL) + (XL - shoulderXL) * (XL - shoulderXL); // r^2

    x = zeta * zeta + r2 - L1 * L1 - L2 * L2;
    y = sqrt(4.0 * L1 * L1 * L2 * L2 - x * x);

    float theta6 = atan2(y, x);

    if (theta6 < 0 || theta6 > 140.0 * M_PI / 180.0) // the6 범위 : 0deg ~ 120deg
    {
        std::cout << "IKFUN (q6) is not solved!!\n";
        state.main = Main::Error;
    }

    float theta56 = atan2(sqrt(r2), zeta);
    float theta5 = theta56 - atan2(L2 * sin(theta6), L1 + L2 * cos(theta6));

    if (theta5 < -45.0 * M_PI / 180.0 || theta5 > 90.0 * M_PI / 180.0) // the5 범위 : -45deg ~ 90deg
    {
        std::cout << "IKFUN (q5) is not solved!!\n";
        state.main = Main::Error;
    }

    Qf.resize(9);
    Qf << theta0, theta1, theta2, theta3, theta4, theta5, theta6, 0, 0;

    return Qf;
}

VectorXd TestManager::calWaistAngle(VectorXd pR, VectorXd pL)
{
    PartLength partLength;

    float XR = pR(0), YR = pR(1), ZR = pR(2);
    float XL = pL(0), YL = pL(1), ZL = pL(2);
    float R1 = partLength.upperArm;
    float R2 = partLength.lowerArm + partLength.stick;
    float L1 = partLength.upperArm;
    float L2 = partLength.lowerArm + partLength.stick;
    float s = partLength.waist;
    float z0 = partLength.height;

    VectorXd W(2);
    W << 2, 1;
    double minCost = W.sum();
    double w = 0, cost = 0;
    int minIndex = 0;

    MatrixXd Qarr(7, 1);
    VectorXd output(3);
    int j = 0;

    for (int i = 0; i < 1801; i++)
    {
        double theta0 = -0.5 * M_PI + M_PI / 1800.0 * i; // the0 범위 : -90deg ~ 90deg

        float shoulderXR = 0.5 * s * cos(theta0);
        float shoulderYR = 0.5 * s * sin(theta0);
        float shoulderXL = -0.5 * s * cos(theta0);
        float shoulderYL = -0.5 * s * sin(theta0);

        float theta01 = atan2(YR - shoulderYR, XR - shoulderXR);
        float theta1 = theta01 - theta0;

        if (theta1 > 0 && theta1 < 150.0 * M_PI / 180.0) // the1 범위 : 0deg ~ 150deg
        {
            float theta02 = atan2(YL - shoulderYL, XL - shoulderXL);
            float theta2 = theta02 - theta0;

            if (theta2 > 30 * M_PI / 180.0 && theta2 < M_PI) // the2 범위 : 30deg ~ 180deg
            {
                float zeta = z0 - ZR;
                float r2 = (YR - shoulderYR) * (YR - shoulderYR) + (XR - shoulderXR) * (XR - shoulderXR); // r^2

                float x = zeta * zeta + r2 - R1 * R1 - R2 * R2;

                if (4.0 * R1 * R1 * R2 * R2 - x * x > 0)
                {
                    float y = sqrt(4.0 * R1 * R1 * R2 * R2 - x * x);

                    float theta4 = atan2(y, x);

                    if (theta4 > 0 && theta4 < 140.0 * M_PI / 180.0) // the4 범위 : 0deg ~ 120deg
                    {
                        float theta34 = atan2(sqrt(r2), zeta);
                        float theta3 = theta34 - atan2(R2 * sin(theta4), R1 + R2 * cos(theta4));

                        if (theta3 > -45.0 * M_PI / 180.0 && theta3 < 90.0 * M_PI / 180.0) // the3 범위 : -45deg ~ 90deg
                        {
                            zeta = z0 - ZL;
                            r2 = (YL - shoulderYL) * (YL - shoulderYL) + (XL - shoulderXL) * (XL - shoulderXL); // r^2

                            x = zeta * zeta + r2 - L1 * L1 - L2 * L2;

                            if (4.0 * L1 * L1 * L2 * L2 - x * x > 0)
                            {
                                y = sqrt(4.0 * L1 * L1 * L2 * L2 - x * x);

                                float theta6 = atan2(y, x);

                                if (theta6 > 0 && theta6 < 140.0 * M_PI / 180.0) // the6 범위 : 0deg ~ 120deg
                                {
                                    float theta56 = atan2(sqrt(r2), zeta);
                                    float theta5 = theta56 - atan2(L2 * sin(theta6), L1 + L2 * cos(theta6));

                                    if (theta5 > -45.0 * M_PI / 180.0 && theta5 < 90.0 * M_PI / 180.0) // the5 범위 : -45deg ~ 90deg
                                    {
                                        if (j == 0)
                                        {
                                            Qarr(0, 0) = theta0;
                                            Qarr(1, 0) = theta1;
                                            Qarr(2, 0) = theta2;
                                            Qarr(3, 0) = theta3;
                                            Qarr(4, 0) = theta4;
                                            Qarr(5, 0) = theta5;
                                            Qarr(6, 0) = theta6;

                                            j = 1;
                                        }
                                        else
                                        {
                                            Qarr.conservativeResize(Qarr.rows(), Qarr.cols() + 1);

                                            Qarr(0, j) = theta0;
                                            Qarr(1, j) = theta1;
                                            Qarr(2, j) = theta2;
                                            Qarr(3, j) = theta3;
                                            Qarr(4, j) = theta4;
                                            Qarr(5, j) = theta5;
                                            Qarr(6, j) = theta6;

                                            j++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (j == 0)
    {
        cout << "IKFUN is not solved!! (Waist Range)\n";
        state.main = Main::Error;

        output(0) = 0;
        output(1) = 0;
    }
    else
    {
        output(0) = Qarr(0, 0);     // min
        output(1) = Qarr(0, j - 1); // max
    }

    w = 2.0 * M_PI / abs(Qarr(0, j - 1) - Qarr(0, 0));
    for (int i = 0; i < j; i++)
    {
        cost = W(0)*cos(Qarr(1, i) + Qarr(2, i)) + W(1)*cos(w*abs(Qarr(0, i) - Qarr(0, 0)));

        if (cost < minCost)
        {
            minCost = cost;
            minIndex = i;
        }
    }
    output(2) = Qarr(0, minIndex);

    return output;
}

void TestManager::testTmotorVelocityMode()
{
    while(true)
    {
        int userInput = 100;
        int ret = system("clear");
        if (ret == -1) std::cout << "system clear error" << endl;

        float c_MotorAngle[10] = {0};
        getMotorPos(c_MotorAngle);

        std::cout << "[ Current Q Values (Radian / Degree) ]\n";
        for (int i = 0; i < 10; i++)
        {
            std::cout << "Q[" << i << "] : " << c_MotorAngle[i] << "\t\t" << c_MotorAngle[i] * 180.0 / M_PI << "\t\t" <<q[i]/ M_PI * 180.0 << "\n";
        }

        std::cout << "\ntime : " << t << "s";
        std::cout << "\nnumber of repeat : " << n_repeat << std::endl << std::endl;


        std::cout << "\nSelect Motor to Change Value (0-8) / Run (9) / Time (10) / Extra Time (11) / Repeat(12) / Exit (-1): ";
        std::cin >> userInput;

        if (userInput == -1)
        {
            break;
        }
        else if (userInput < 9)
        {
            float degree_angle;

            std::cout << "\nRange : " << jointRangeMin[userInput] << "~" << jointRangeMax[userInput] << "(Degree)\n";
            std::cout << "Enter q[" << userInput << "] Values (Degree) : ";
            std::cin >> degree_angle;
            q[userInput] = degree_angle * M_PI / 180.0;
        }
        else if (userInput == 9)
        {
            for (auto &motor_pair : motors)
            {
                if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(motor_pair.second))
                {
                    tMotor->clearCommandBuffer();
                    tMotor->clearReceiveBuffer();
                }
                else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(motor_pair.second))
                {
                    maxonMotor->clearCommandBuffer();
                    maxonMotor->clearReceiveBuffer();
                }
            }
            pushVelCmd(q);
        }
        else if (userInput == 10)
        {
            std::cout << "time : ";
            std::cin >> t;
        }
        else if (userInput == 11)
        {
            std::cout << "extra time : ";
            std::cin >> extra_time;
        }
        else if (userInput == 12)
        {
            std::cout << "number of repeat : ";
            std::cin >> n_repeat;
        }
    }
}

void TestManager::pushVelCmd(float arr[])
{
    const float acc_max = 100.0;    // rad/s^2
    vector<float> Qi;
    vector<float> Vi;
    vector<float> Vmax;
    float Q1[12] = {0.0};
    float Q2[12] = {0.0};
    int n;
    int n_p;    // 목표위치까지 가기 위한 추가 시간

    n = (int)(t/canManager.DTSECOND);    // t초동안 이동
    n_p = (int)(extra_time/canManager.DTSECOND);  // 추가 시간
    
    std::cout << "Get Array...\n";

    getMotorPos(Q1);

    for (int i = 0; i < 12; i++)
    {
        Q2[i] = arr[i];
    }

    Vmax = cal_Vmax(Q1, Q2, acc_max, t);
    
    for (int i = 0; i < n_repeat; i++)
    {
        for (int k = 1; k <= n + n_p; ++k)
        {
            // Make Vector
            if ((i%2) == 0)
            {
                Qi = makeProfile(Q1, Q2, Vmax, acc_max, t*k/n, t);
                Vi = makeVelProfile(Q1, Q2, Vmax, acc_max, t*k/n, t);
            }
            else
            {
                Qi = makeProfile(Q2, Q1, Vmax, acc_max, t*k/n, t);
                Vi = makeVelProfile(Q2, Q1, Vmax, acc_max, t*k/n, t);
            }

            // Send to Buffer
            for (auto &entry : motors)
            {
                if (std::shared_ptr<TMotor> tMotor = std::dynamic_pointer_cast<TMotor>(entry.second))
                {
                    TMotorData newData;
                    newData.position = tMotor->jointAngleToMotorPosition(Qi[canManager.motorMapping[entry.first]]);
                    newData.mode = tMotor->Velocity;
                    newData.velocityERPM = Vi[canManager.motorMapping[entry.first]] * 60.0 * 21.0 * 10 / 2.0 / M_PI;
                    {
                        std::lock_guard<std::mutex> lock(tMotor->bufferMutex);
                        tMotor->commandBuffer.push(newData);
                    }
                    
                    tMotor->finalMotorPosition = newData.position;
                }
                else if (std::shared_ptr<MaxonMotor> maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(entry.second))
                {
                    MaxonData newData;
                    newData.position = maxonMotor->jointAngleToMotorPosition(Qi[canManager.motorMapping[entry.first]]);
                    newData.mode = maxonMotor->CSP;
                    {
                        std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);
                        maxonMotor->commandBuffer.push(newData);
                    }

                    maxonMotor->finalMotorPosition = newData.position;
                }
            }
        }
    }
}

vector<float> TestManager::makeVelProfile(float q1[], float q2[], vector<float> &Vmax, float acc, float t, float t2)
{
    vector<float> Vi;
    for(long unsigned int i = 0; i < 12; i++)
    {
        float val, S;
        int sign;
        S = q2[i] - q1[i];   
        // 부호 확인
        if (S < 0)
        {
            S = -1 * S;
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
            val = 0;
        }
        else if (Vmax[i] < 0)
        {
            // Vmax 값을 구하지 못했을 때 삼각형 프로파일 생성
            float acc_tri = 4 * S / t2 / t2;
            if (t < t2/2)
            {
                val = sign * acc_tri * t;
            }
            else if (t < t2)
            {
                val = sign * acc_tri * (t2 - t);
            }
            else
            {
                val = 0;
            }
        }
        else
        {
            // 사다리꼴 프로파일
            if (t < Vmax[i] / acc)
            {
                // 가속
                val = sign * acc * t;
            }
            else if (t < S / Vmax[i])
            {
                // 등속
                val = sign * Vmax[i];          
            }
            else if (t < Vmax[i] / acc + S / Vmax[i])
            {
                // 감속
                val = sign * acc * (S / Vmax[i] + Vmax[i] / acc - t);          
            }
            else 
            {
                val = 0;
            }
        }
        Vi.push_back(val);
    }
    return Vi;
}

 /////////////////////////////////////////////////////////////////////////////////
/*                                  Drum Scan                                  */
/////////////////////////////////////////////////////////////////////////////////

void TestManager::DrumScan(float Waist_angle)
{
    try {
        // --- 초기 설정 ---
        auto dictionary_name = aruco::DICT_4X4_50;
        
        rs2::pipeline pipe;
        rs2::config cfg;

        ofstream outFile;

        int rgb_width = 1280;
        int rgb_height = 720;

        // USB3.0 사용 시 부활!
        // int rgb_fps = 30;
        // int depth_width = 848;
        // int depth_height = 480;
        // int depth_fps = 30;

        cfg.enable_stream(RS2_STREAM_COLOR, rgb_width, rgb_height, RS2_FORMAT_BGR8, 15);
        cfg.enable_stream(RS2_STREAM_DEPTH, 480, 270, RS2_FORMAT_Z16, 6);
        
        rs2::pipeline_profile profile = pipe.start(cfg);
        
        // Depth를 Color 시점으로 정렬
        rs2::align align_to_color(RS2_STREAM_COLOR);

        // [중요] Color 스트림의 내부 파라미터 가져오기 (영점 보정용)
        rs2_intrinsics intrin = profile.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>().get_intrinsics();

        // ArUco 설정
        Ptr<aruco::Dictionary> dictionary = aruco::getPredefinedDictionary(dictionary_name);
        Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();

        struct DrumTarget { Point3D left_hand; Point3D right_hand; };
        map<int, DrumTarget> drum_map;

        cout << "========== 드럼 스캔 (허리 각도: " << Waist_angle << ") ==========" << endl;
        cout << "종료하려면 창을 클릭하고 'q'를 누르세요." << endl;

        int frame_count = 0;

        while (true) {
            // 프레임 대기 (5초 타임아웃으로 멈춤 방지)
            rs2::frameset frames;
            try {
                frames = pipe.wait_for_frames(5000);
            } catch (...) {
                continue;
            }
            
            frames = align_to_color.process(frames);

            rs2::video_frame color_frame = frames.get_color_frame();
            rs2::depth_frame depth_frame = frames.get_depth_frame();
            if (!color_frame || !depth_frame) continue;

            Mat color_image(Size(rgb_width, rgb_height), CV_8UC3, (void*)color_frame.get_data());

            // 마커 감지
            vector<int> ids;
            vector<vector<Point2f>> corners;
            aruco::detectMarkers(color_image, dictionary, corners, ids, parameters);

            // [시각화 1] 화면 중앙 십자가 그리기
            float center_x = rgb_width / 2.0f;
            float center_y = rgb_height / 2.0f;
            line(color_image, Point(center_x, 0), Point(center_x, rgb_height), Scalar(0, 0, 255), 1);
            line(color_image, Point(0, center_y), Point(rgb_width, center_y), Scalar(0, 0, 255), 1);

            // [시각화 2] 중앙점 좌표 디버깅
            float center_dist = depth_frame.get_distance((int)center_x, (int)center_y);
            if (center_dist > 0) {
                float c_pixel[2] = { center_x, center_y };
                float c_point[3];
                rs2_deproject_pixel_to_point(c_point, &intrin, c_pixel, center_dist);

                string c_text = format("Center: (%.3f, %.3f, %.3f)", c_point[0], c_point[1], c_point[2]);
                putText(color_image, c_text, Point(center_x + 10, center_y + 30), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 0), 2);
            }

            // [마커 처리]
            if (ids.size() > 0) {
                aruco::drawDetectedMarkers(color_image, corners, ids);

                for (size_t i = 0; i < ids.size(); ++i) {
                    // 마커 중심 계산
                    Point2f center(0, 0);
                    for (int j = 0; j < 4; j++) center += corners[i][j];
                    center *= 0.25f;

                    float dist = depth_frame.get_distance((int)center.x, (int)center.y);
                    if (dist > 0) {
                        float pixel[2] = { center.x, center.y };
                        float marker_cam[3];
                        rs2_deproject_pixel_to_point(marker_cam, &intrin, pixel, dist);

                        // [시각화 3] 마커 좌표 출력
                        string label = format("X:%.2f Y:%.2f Z:%.2f", marker_cam[0], marker_cam[1], marker_cam[2]);
                        putText(color_image, label, center, FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 0), 2);

                        // 손 오프셋 계산
                        Point3D left_cam_pt = { marker_cam[0] - HAND_OFFSET_X, marker_cam[1] + HAND_OFFSET_Y, marker_cam[2] };
                        Point3D right_cam_pt = { marker_cam[0] + HAND_OFFSET_X, marker_cam[1] + HAND_OFFSET_Y, marker_cam[2] };

                        // 월드 좌표 변환 및 저장
                        drum_map[ids[i]] = { 
                            transform_to_world(left_cam_pt, ROBOT_WAIST_ANGLE, CAMERA_TILT_ANGLE),
                            transform_to_world(right_cam_pt, ROBOT_WAIST_ANGLE, CAMERA_TILT_ANGLE)
                        };
                    }
                }
            }

            // 매 {frame_count}개의 프레임마다 저장
            frame_count++;
            if (frame_count % 15 == 0) {
                outFile.open("../drum_coordinates.txt");
                if (outFile.is_open()) {
                    outFile << fixed << setprecision(6); // 소수점 6자리 고정
                    // 1. 오른손 X
                    for (int id = 1; id < TOTAL_DRUMS; ++id) {
                        if (drum_map.count(id)) outFile << drum_map[id].right_hand.x << "\t";
                        else outFile << 0.0f << "\t";
                    }
                    outFile << endl;

                    // 2. 오른손 Y
                    for (int id = 1; id < TOTAL_DRUMS; ++id) {
                        if (drum_map.count(id)) outFile << drum_map[id].right_hand.y << "\t";
                        else outFile << 0.0f << "\t";
                    }
                    outFile << endl;

                    // 3. 오른손 Z
                    for (int id = 1; id < TOTAL_DRUMS; ++id) {
                        if (drum_map.count(id)) outFile << drum_map[id].right_hand.z << "\t";
                        else outFile << 0.0f << "\t";
                    }
                    outFile << endl;

                    // 4. 왼손 X
                    for (int id = 1; id < TOTAL_DRUMS; ++id) {
                        if (drum_map.count(id)) outFile << drum_map[id].left_hand.x << "\t";
                        else outFile << 0.0f << "\t";
                    }
                    outFile << endl;

                    // 5. 왼손 Y
                    for (int id = 1; id < TOTAL_DRUMS; ++id) {
                        if (drum_map.count(id)) outFile << drum_map[id].left_hand.y << "\t";
                        else outFile << 0.0f << "\t";
                    }
                    outFile << endl;

                    // 6. 왼손 Z
                    for (int id = 1; id < TOTAL_DRUMS; ++id) {
                        if (drum_map.count(id)) outFile << drum_map[id].left_hand.z << "\t";
                        else outFile << 0.0f << "\t";
                    }
                    outFile << endl;
                }
                else{ std::cout << " file isn't open " << endl;cerr << "\n[에러] 파일 생성 실패! 경로 권한을 확인하세요." << endl;}
                cout << "\r[저장완료] 감지된 드럼 수: " << drum_map.size() << " / " << TOTAL_DRUMS << "   " << flush;
            }

            imshow("Robot Eye View", color_image);
            if (waitKey(1) == 'q') break;
        }
        cv::destroyAllWindows(); // 열린 창 닫기
        pipe.stop();             // 카메라 스트리밍 중지
        return;
    }
    catch (const rs2::error & e) {
        cerr << "RealSense Error: " << e.what() << endl;
        return;
    }
    catch (const exception & e) {
        cerr << "Error: " << e.what() << endl;
        return;
    }
}

TestManager::Point3D TestManager::transform_to_world(Point3D cam_pt, float waist_deg, float tilt_deg)
{
    const float PI = 3.14159265f;
    float theta = tilt_deg * PI / 180.0f;
    float psi = waist_deg * PI / 180.0f;

    // 0. cam_pt: 카메라 좌표축 기준 좌표값

    // 1. 아래로 숙인 카메라 각도 보정
    float y_untilted = cam_pt.y * cos(theta) - cam_pt.z * sin(theta);
    float z_untilted = cam_pt.y * sin(theta) + cam_pt.z * cos(theta);
    float x_untilted = cam_pt.x;

    // 2. world 좌표로 변환하기 위해 좌표축 변환(카메라 좌표는 화면 좌상단이 원점, 오른쪽 가로방향: +x, 아래 세로방향: +y, 깊이: +z)
    float x_body = x_untilted;
    float y_body = z_untilted;
    float z_body = -y_untilted;

    // 3. 허리 회전 각도 보정
    float x_world = x_body * cos(psi) - (y_body + CAMERA_OFFSET_FWD) * sin(psi);
    float y_world = x_body * sin(psi) + (y_body + CAMERA_OFFSET_FWD) * cos(psi);
    float z_world = z_body + CAMERA_HEIGHT;

    // cout << "------------------------------------------------" << endl;
    // cout << "[1.Input Cam] X:" << cam_pt.x << "\t Y:" << cam_pt.y << "\t Z:" << cam_pt.z << endl;
    // cout << "[2.Untilted] X:" << x_untilted << "\t Y:" << y_untilted << "\t Z:" << z_untilted << endl;
    // cout << "[3.Body Frame] X:" << x_body << "\t Y:" << y_body << "\t Z:" << z_body << endl;
    // cout << "[4.Final World] X:" << x_world << "\t Y:" << y_world << "\t Z:" << z_world << endl;
    // cout << "------------------------------------------------" << endl;

    return {x_world, y_world, z_world};
}

cv::Mat TestManager::getIdentity() 
{
    return cv::Mat::eye(4, 4, CV_64F);
}

// rvec, tvec를 4x4 변환 행렬로 변환
cv::Mat TestManager::getTransformMatrix(const cv::Vec3d& rvec, const cv::Vec3d& tvec) 
{
    cv::Mat R;
    cv::Rodrigues(rvec, R);
    
    cv::Mat T = cv::Mat::eye(4, 4, CV_64F);
    R.copyTo(T(cv::Rect(0, 0, 3, 3)));
    T.at<double>(0, 3) = tvec[0];
    T.at<double>(1, 3) = tvec[1];
    T.at<double>(2, 3) = tvec[2];
    
    return T;
}

// 마커의 월드 기준 포즈 행렬 생성 (지면에 평평, Y축 방향 정렬 가정)
cv::Mat TestManager::getMarkerWorldPose(double x, double y, double z) 
{
    // 마커 좌표계: 빨간색(x) 오른쪽, 초록색(y) 앞쪽, 파란색(z) 위쪽
    // 월드 좌표계와 방향이 일치한다고 가정 (Identity Rotation)
    cv::Mat T = cv::Mat::eye(4, 4, CV_64F);
    T.at<double>(0, 3) = x;
    T.at<double>(1, 3) = y;
    T.at<double>(2, 3) = z;
    return T;
}

// 행렬 출력용 헬퍼
void TestManager::printMatrix(const std::string& name, const cv::Mat& M) 
{
    std::cout << "[" << name << "]" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "  ";
        for (int j = 0; j < 4; j++) {
            printf("%7.3f ", M.at<double>(i, j));
        }
        std::cout << std::endl;
    }
}

cv::Vec3d TestManager::getEulerAngles(cv::Mat R_in) 
{
    // 3x3 행렬이 아니면 예외 처리 필요하지만 여기선 생략
    cv::Mat mtxR, mtxQ;
    // RQ 분해를 이용해 Euler Angle (x, y, z 축 회전) 추출
    cv::Vec3d angles = cv::RQDecomp3x3(R_in, mtxR, mtxQ);
    return angles;
}

void TestManager::camera_calibration(float CURRENT_WAIST_ANGLE_DEG)
{
    try {

        struct MarkerPos { float x, y, z; };
        std::map<int, MarkerPos> KNOWN_MARKERS = {
            // {ID, {x, y, z}}
            {0, {-0.783, 0.00, 0.08}},  // ID 0번 마커 위치
            {1, {-0.693, 0.00, 0.08}},  // ID 1번 마커 위치
            {2, {-0.603, 0.00, 0.08}}   // ID 2번 마커 위치
        };

        // --- 1. RealSense 초기화 ---
        rs2::pipeline pipe;
        rs2::config cfg;
        cfg.enable_stream(RS2_STREAM_COLOR, 848, 480, RS2_FORMAT_BGR8, 30);
        rs2::pipeline_profile profile = pipe.start(cfg);

        // Intrinsic 가져오기
        auto stream = profile.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();
        rs2_intrinsics intr = stream.get_intrinsics();

        cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
        cameraMatrix.at<float>(0, 0) = intr.fx;    // x축 초점거리
        cameraMatrix.at<float>(1, 1) = intr.fy;    // y축 초점거리
        cameraMatrix.at<float>(0, 2) = intr.ppx;   // 주점의 x좌표
        cameraMatrix.at<float>(1, 2) = intr.ppy;   // 주점의 y좌표

        cv::Mat distCoeffs = cv::Mat::zeros(5, 1, CV_64F);
        for(int i=0; i<5; i++) distCoeffs.at<float>(i) = intr.coeffs[i];   // 왜곡 계수 배열

        // --- 2. Aruco 설정 (OpenCV 4.2.0 호환) ---
        cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
        cv::Ptr<cv::aruco::DetectorParameters> parameters = cv::aruco::DetectorParameters::create();

        std::cout << "=== Camera Calibration Started ===" << std::endl;
        std::cout << "Current Waist Angle: " << CURRENT_WAIST_ANGLE_DEG << " deg" << std::endl;
        std::cout << "Press 'ESC' to exit." << std::endl;

        auto last_print_time = std::chrono::steady_clock::now();

        while (true) {
            rs2::frameset frames = pipe.wait_for_frames();
            rs2::frame color_frame = frames.get_color_frame();
            if (!color_frame) continue;

            cv::Mat image(cv::Size(848, 480), CV_8UC3, (void*)color_frame.get_data(), cv::Mat::AUTO_STEP);
            
            std::vector<int> ids;
            std::vector<std::vector<cv::Point2f>> corners;
            cv::aruco::detectMarkers(image, dictionary, corners, ids, parameters);

            std::vector<cv::Vec3d> rvecs, tvecs;
            if (ids.size() > 0) {
                cv::aruco::estimatePoseSingleMarkers(corners, 0.08, cameraMatrix, distCoeffs, rvecs, tvecs);
                
                auto current_time = std::chrono::steady_clock::now();
                std::chrono::duration<float> elapsed_seconds = current_time - last_print_time;

                // 다중 마커를 이용한 카메라 위치 누적 계산용
                cv::Mat T_World_Camera_Sum = cv::Mat::zeros(4, 4, CV_64F);
                int valid_marker_count = 0;

                for (size_t i = 0; i < ids.size(); ++i) {
                    int id = ids[i];
                    
                    // 1. 우리가 아는 마커인지 확인
                    if (KNOWN_MARKERS.find(id) != KNOWN_MARKERS.end()) {
                        cv::aruco::drawAxis(image, cameraMatrix, distCoeffs, rvecs[i], tvecs[i], 0.03);

                        // A. 카메라 기준 마커 포즈 (T_Camera_Marker)
                        cv::Mat T_Camera_Marker = getTransformMatrix(rvecs[i], tvecs[i]);

                        // B. 월드 기준 마커 포즈 (T_World_Marker) - 정의된 값
                        MarkerPos pos = KNOWN_MARKERS[id];
                        cv::Mat T_World_Marker = getMarkerWorldPose(pos.x, pos.y, pos.z);

                        // C. 월드 기준 카메라 포즈 계산 (T_World_Camera)
                        // 수식: T_World_Camera = T_World_Marker * (T_Camera_Marker)^-1
                        cv::Mat T_World_Camera = T_World_Marker * T_Camera_Marker.inv();

                        T_World_Camera_Sum += T_World_Camera;
                        valid_marker_count++;
                    }
                }

                // 2. 평균 내서 최종 카메라 위치 도출
                if (valid_marker_count > 0) {
                    cv::Mat T_Final = T_World_Camera_Sum / valid_marker_count;
                    
                    // 1. 회전 행렬 추출 (4x4 행렬의 좌상단 3x3)
                    cv::Mat RotationMatrix = T_Final(cv::Rect(0, 0, 3, 3));
                    
                    // 2. 각도 계산 함수 호출
                    cv::Vec3d angles = getEulerAngles(RotationMatrix);

                    // 3. 화면에 각도 출력 (X:Pitch, Y:Yaw, Z:Roll)
                    std::string angle_text = cv::format("Angle(deg): X=%.2f Y=%.2f Z=%.2f", 
                        angles[0], angles[1], angles[2]);
                    
                    // 회전된 뷰에 텍스트 출력 (위치: y=90 지점)
                    cv::putText(image, angle_text, cv::Point(10, 90), 
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2); // 노란색

                    // 화면 출력 (Overlays)
                    std::string pos_text = cv::format("Cam Pos(m): x=%.2f y=%.2f z=%.2f", 
                        T_Final.at<float>(0,3), T_Final.at<float>(1,3), T_Final.at<float>(2,3));
                    cv::putText(image, pos_text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
                    
                    std::string count_text = cv::format("Markers used: %d", valid_marker_count);
                    cv::putText(image, count_text, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);

                    // 콘솔에 상세 출력 (가독성을 위해 1초에 한 번만 출력하거나 필요시 주석 해제)
                    // printMatrix("Calculated Camera Pose in WORLD", T_Final);

                    // --- [추가] 허리 회전 고려한 고정 오프셋 계산 (Reference용) ---
                    // 허리 회전 행렬 (Z축 회전)
                    float rad = CURRENT_WAIST_ANGLE_DEG * CV_PI / 180.0;
                    cv::Mat T_Waist_Rot = cv::Mat::eye(4, 4, CV_64F);
                    T_Waist_Rot.at<float>(0, 0) = cos(rad);
                    T_Waist_Rot.at<float>(0, 1) = -sin(rad);
                    T_Waist_Rot.at<float>(1, 0) = sin(rad);
                    T_Waist_Rot.at<float>(1, 1) = cos(rad);

                    // T_World_Camera = T_Waist_Rotation * T_Offset
                    // 따라서 T_Offset = (T_Waist_Rotation)^-1 * T_World_Camera
                    cv::Mat T_Offset = T_Waist_Rot.inv() * T_Final;

                    cv::Mat RotationMatrix_Offset = T_Offset(cv::Rect(0, 0, 3, 3));
                    cv::Vec3d angles_offset = getEulerAngles(RotationMatrix_Offset);
                    
                    if (elapsed_seconds.count() > 0.5) { 
                        std::cout << "\n================ [Status Update] ================" << std::endl;
                        
                        // 1. 월드(마커) 기준 카메라의 절대 회전 (로봇 움직이면 변함)
                        std::cout << "[World  ] Pitch: " << angles[0] 
                                << " / Yaw: " << angles[1] 
                                << " / Roll: " << angles[2] << std::endl;
                        
                        // 2. 로봇(허리) 기준 카메라의 장착 각도 (고정값, 우리가 원하는 것)
                        std::cout << "[ Offset] Pitch: " << angles_offset[0] 
                                << " / Yaw: " << angles_offset[1] 
                                << " / Roll: " << angles_offset[2] << std::endl;

                        // 3. 로봇(허리) 기준 카메라의 장착 위치 (고정값)
                        std::cout << "[ Offset] Pos(m): X=" << T_Offset.at<float>(0,3) 
                                << " Y=" << T_Offset.at<float>(1,3) 
                                << " Z=" << T_Offset.at<float>(2,3) << std::endl;
                        
                        std::cout << "=================================================\n" << std::endl;

                        // [중요] 마지막 출력 시간을 현재 시간으로 갱신
                        last_print_time = current_time;
                    }
                }
            }

            cv::Mat rotated_view;

            cv::rotate(image, rotated_view, cv::ROTATE_90_COUNTERCLOCKWISE);
            cv::imshow("Rotated eye view", rotated_view); // 원본 대신 회전된 이미지 출력


            cv::imshow("Robot Camera Calibration", image);
            if (cv::waitKey(1) == 27) break; // ESC to exit
        }

    } catch (const rs2::error & e) {
        std::cerr << "RealSense Error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    cv::destroyAllWindows(); 
    cv::waitKey(1);

    // return 0;
}
