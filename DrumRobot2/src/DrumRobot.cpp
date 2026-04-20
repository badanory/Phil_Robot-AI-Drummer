// DrumRobot2/src/DrumRobot.cpp

#include "../include/tasks/DrumRobot.hpp"
#include "../include/managers/SilCommandPipeWriter.hpp"

using namespace std;

// DrumRobot 클래스의 생성자
DrumRobot::DrumRobot(State &stateRef,
                     CanManager &canManagerRef,
                     PathManager &pathManagerRef,
                     TestManager &testManagerRef,
                     map<string, shared_ptr<GenericMotor>> &motorsRef,
                     USBIO &usbioRef,
                     Functions &funRef)
    : state(stateRef),
      canManager(canManagerRef),
      pathManager(pathManagerRef),
      testManager(testManagerRef),
      motors(motorsRef),
      usbio(usbioRef),
      func(funRef),
      agentAction(pathManagerRef, flagObj, arduino, motorsRef)
{
    sendLoopPeriod = chrono::steady_clock::now();
    recvLoopPeriod = chrono::steady_clock::now();
}

////////////////////////////////////////////////////////////////////////////////
/*                          Initialize DrumRobot                              */
////////////////////////////////////////////////////////////////////////////////

void DrumRobot::initializeMotors()
{
    motors["waist"] = make_shared<TMotor>(0x00, "AK10_9");
    motors["R_arm1"] = make_shared<TMotor>(0x01, "AK70_10");
    motors["L_arm1"] = make_shared<TMotor>(0x02, "AK70_10");
    motors["R_arm2"] = make_shared<TMotor>(0x03, "AK70_10");
    motors["R_arm3"] = make_shared<TMotor>(0x04, "AK70_10");
    motors["L_arm2"] = make_shared<TMotor>(0x05, "AK70_10");
    motors["L_arm3"] = make_shared<TMotor>(0x06, "AK70_10");
    motors["R_wrist"] = make_shared<MaxonMotor>(0x07);
    motors["L_wrist"] = make_shared<MaxonMotor>(0x08); 
    motors["maxonForTest"] = make_shared<MaxonMotor>(0x09);
    motors["R_foot"] = make_shared<MaxonMotor>(0x0A);
    motors["L_foot"] = make_shared<MaxonMotor>(0x0B);
    
    for (auto &motor_pair : motors)
    {
        auto &motor = motor_pair.second;
        int can_id = canManager.motorMapping[motor_pair.first];

        // 타입에 따라 적절한 캐스팅과 초기화 수행
        if (shared_ptr<TMotor> tMotor = dynamic_pointer_cast<TMotor>(motor))
        {
            // 각 모터 이름에 따른 멤버 변수 설정
            if (motor_pair.first == "waist")
            {
                tMotor->cwDir = 1.0f;
                tMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -90deg
                tMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f;  // 90deg
                tMotor->myName = "waist";
                tMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
                tMotor->currentLimit = 29.8;  // [A]    // ak10-9
                tMotor->useFourBarLinkage = false;
                tMotor->gearRatio = 9.0;
                tMotor->positionGain = 1000.0;
            }
            else if (motor_pair.first == "R_arm1")
            {
                tMotor->cwDir = -1.0f;
                tMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f;   // 0deg
                tMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f; // 150deg
                tMotor->myName = "R_arm1";
                tMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
                tMotor->currentLimit = 23.2;  // [A]    // ak70-10
                tMotor->useFourBarLinkage = false;
                tMotor->gearRatio = 10.0;
                tMotor->positionGain = 1000.0;
            }
            else if (motor_pair.first == "L_arm1")
            {
                tMotor->cwDir = -1.0f;
                tMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f;  // 30deg
                tMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f; // 180deg
                tMotor->myName = "L_arm1";
                tMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
                tMotor->currentLimit = 23.2;  // [A]    // ak70-10
                tMotor->useFourBarLinkage = false;
                tMotor->gearRatio = 10.0;
                tMotor->positionGain = 1000.0;
            }
            else if (motor_pair.first == "R_arm2")
            {
                tMotor->cwDir = 1.0f;
                tMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -60deg
                tMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f;  // 90deg
                tMotor->myName = "R_arm2";
                tMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
                tMotor->currentLimit = 23.2;  // [A]    // ak70-10
                tMotor->useFourBarLinkage = false;
                tMotor->gearRatio = 10.0;
                tMotor->positionGain = 4000.0;
            }
            else if (motor_pair.first == "R_arm3")
            {
                tMotor->cwDir = -1.0f;
                tMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -30deg
                tMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f; // 130deg
                tMotor->myName = "R_arm3";
                tMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
                tMotor->currentLimit = 23.2;  // [A]    // ak70-10
                tMotor->useFourBarLinkage = false;
                // tMotor->setInitialMotorAngle(tMotor->initialJointAngle);     // 4-bar-linkage 사용하면 쓰는 함수
                tMotor->gearRatio = 10.0;
                tMotor->positionGain = 4000.0;
            }
            else if (motor_pair.first == "L_arm2")
            {
                tMotor->cwDir = -1.0f;
                tMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -60deg
                tMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f;  // 90deg
                tMotor->myName = "L_arm2";
                tMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
                tMotor->currentLimit = 23.2;  // [A]    // ak70-10
                tMotor->useFourBarLinkage = false;
                tMotor->gearRatio = 10.0;
                tMotor->positionGain = 1000.0;
            }
            else if (motor_pair.first == "L_arm3")
            {
                tMotor->cwDir = 1.0f;
                tMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -30 deg
                tMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f; // 130 deg
                tMotor->myName = "L_arm3";
                tMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
                tMotor->currentLimit = 23.2;  // [A]    // ak70-10
                tMotor->useFourBarLinkage = false;
                // tMotor->setInitialMotorAngle(tMotor->initialJointAngle);     // 4-bar-linkage 사용하면 쓰는 함수
                tMotor->gearRatio = 10.0;
                tMotor->positionGain = 4000.0;
            }
        }
        else if (shared_ptr<MaxonMotor> maxonMotor = dynamic_pointer_cast<MaxonMotor>(motor))
        {
            // 각 모터 이름에 따른 멤버 변수 설정
            if (motor_pair.first == "R_wrist")
            {
                maxonMotor->cwDir = -1.0f;
                maxonMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -108deg
                maxonMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f;  // 135deg
                maxonMotor->txPdoIds[0] = 0x207; // Controlword
                maxonMotor->txPdoIds[1] = 0x307; // TargetPosition
                maxonMotor->txPdoIds[2] = 0x407; // TargetVelocity
                maxonMotor->txPdoIds[3] = 0x507; // TargetTorque
                maxonMotor->rxPdoIds[0] = 0x187; // Statusword, ActualPosition, ActualTorque
                maxonMotor->rxPdoIds[1] = 0x287; // ActualPosition, ActualTorque
                maxonMotor->myName = "R_wrist";
                maxonMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
            }
            else if (motor_pair.first == "L_wrist")
            {
                maxonMotor->cwDir = -1.0f;
                maxonMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -108deg
                maxonMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f;  // 135deg
                maxonMotor->txPdoIds[0] = 0x208; // Controlword
                maxonMotor->txPdoIds[1] = 0x308; // TargetPosition
                maxonMotor->txPdoIds[2] = 0x408; // TargetVelocity
                maxonMotor->txPdoIds[3] = 0x508; // TargetTorque
                maxonMotor->rxPdoIds[0] = 0x188; // Statusword, ActualPosition, ActualTorque
                maxonMotor->rxPdoIds[1] = 0x288; // ActualPosition, ActualTorque
                maxonMotor->myName = "L_wrist";
                maxonMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
            }
            else if (motor_pair.first == "R_foot")
            {
                maxonMotor->cwDir = 1.0f;
                maxonMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -90deg
                maxonMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f; // 135deg
                maxonMotor->txPdoIds[0] = 0x20A; // Controlword
                maxonMotor->txPdoIds[1] = 0x30A; // TargetPosition
                maxonMotor->txPdoIds[2] = 0x40A; // TargetVelocity
                maxonMotor->txPdoIds[3] = 0x50A; // TargetTorque
                maxonMotor->rxPdoIds[0] = 0x18A; // Statusword, ActualPosition, ActualTorque
                maxonMotor->rxPdoIds[1] = 0x28A; // ActualPosition, ActualTorque
                maxonMotor->myName = "R_foot";
                maxonMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
            }
            else if (motor_pair.first == "L_foot")
            {
                maxonMotor->cwDir = -1.0f;
                maxonMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -90deg
                maxonMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f; // 135deg
                maxonMotor->txPdoIds[0] = 0x20B; // Controlword
                maxonMotor->txPdoIds[1] = 0x30B; // TargetPosition
                maxonMotor->txPdoIds[2] = 0x40B; // TargetVelocity
                maxonMotor->txPdoIds[3] = 0x50B; // TargetTorque
                maxonMotor->rxPdoIds[0] = 0x18B; // Statusword, ActualPosition, ActualTorque
                maxonMotor->rxPdoIds[1] = 0x28B; // ActualPosition, ActualTorque
                maxonMotor->myName = "L_foot";
                maxonMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
            }
            else if (motor_pair.first == "maxonForTest")
            {
                maxonMotor->cwDir = 1.0f;
                maxonMotor->rMin = jointRangeMin[can_id] * M_PI / 180.0f; // -108deg
                maxonMotor->rMax = jointRangeMax[can_id] * M_PI / 180.0f; // 135deg
                maxonMotor->txPdoIds[0] = 0x209; // Controlword
                maxonMotor->txPdoIds[1] = 0x309; // TargetPosition
                maxonMotor->txPdoIds[2] = 0x409; // TargetVelocity
                maxonMotor->txPdoIds[3] = 0x509; // TargetTorque
                maxonMotor->rxPdoIds[0] = 0x189; // Statusword, ActualPosition, ActualTorque
                maxonMotor->rxPdoIds[1] = 0x289; // ActualPosition, ActualTorque
                maxonMotor->myName = "maxonForTest";
                maxonMotor->initialJointAngle = initialJointAngles[can_id] * M_PI / 180.0f;
            }
        }
    }
}

void DrumRobot::initializeCanManager()
{
    canManager.initializeCAN();
    canManager.checkCanPortsStatus();
    allMotorsUnConected = canManager.setMotorsSocket(); // 연결된 모터 없음 : 테스트 모드

    // SIL 모드: vcan0 을 열고 disconnected 모터에 할당한다.
    // Python SilCommandPipeReader 가 tick 후 vcan0 으로 joint state 피드백을 보내면
    // recv loop 가 motor.jointAngle 을 갱신해 close-loop 가 완성된다.
    canManager.openSilVcan("vcan0");

    // vcan 피드백 경로가 열렸으면 "연결됨"으로 갱신한다.
    // allMotorsUnConected=true 이면 sendLoopForThread 에서 is_fixed 가
    // 항상 true 로 강제돼 홈 복귀 오발이 일어나므로, SIL 에서도 반드시 해제한다.
    if (canManager.isSilModeEnabled())
        allMotorsUnConected = false;
}

void DrumRobot::motorSettingCmd()
{
    virtualMaxonMotor.clear();

    // Count Maxon Motors
    for (const auto &motor_pair : motors)
    {
        // 각 요소가 MaxonMotor 타입인지 확인
        if (shared_ptr<MaxonMotor> maxonMotor = dynamic_pointer_cast<MaxonMotor>(motor_pair.second))
        {

            if (virtualMaxonMotor.size() == 0)
            {
                virtualMaxonMotor.push_back(maxonMotor);
            }
            else
            {
                bool otherSocket = true;
                int n = virtualMaxonMotor.size();
                for(int i = 0; i < n; i++)
                {
                    if (virtualMaxonMotor[i]->socket == maxonMotor->socket)
                    {
                        otherSocket = false;
                    }
                }

                if (otherSocket)
                {
                    virtualMaxonMotor.push_back(maxonMotor);
                }
            }
        }
    }

    struct can_frame frame;
    canManager.setSocketsTimeout(2, 0);

    for (const auto &motorPair : motors)
    {
        string name = motorPair.first;
        shared_ptr<GenericMotor> motor = motorPair.second;
        if (shared_ptr<MaxonMotor> maxonMotor = dynamic_pointer_cast<MaxonMotor>(motorPair.second))
        {
            if (canManager.isSilModeEnabled() && !maxonMotor->isConected)
            {
                std::cout << "[SIL] Skip Maxon setup for disconnected motor [" << name << "]" << std::endl;
                continue;
            }

            // CSP Settings
            maxoncmd.getCSPMode(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            maxoncmd.getPosOffset(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            maxoncmd.getTorqueOffset(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            // CSV Settings
            maxoncmd.getCSVMode(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            maxoncmd.getVelOffset(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            // CST Settings
            maxoncmd.getCSTMode(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            maxoncmd.getTorqueOffset(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            // HMM Settigns
            maxoncmd.getHomeMode(*maxonMotor, &frame);
            canManager.sendAndRecv(motor, frame);

            if(name == "L_wrist")
            {
                maxoncmd.getHomingMethodL(*maxonMotor, &frame);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomeoffsetDistance(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomePosition(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                // maxoncmd.getCurrentThresholdL(*maxonMotor, &frame);
                // canManager.sendAndRecv(motor, frame);
            }
            else if (name == "R_wrist")
            {
                maxoncmd.getHomingMethodR(*maxonMotor, &frame);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomeoffsetDistance(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomePosition(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                // maxoncmd.getCurrentThresholdR(*maxonMotor, &frame);
                // canManager.sendAndRecv(motor, frame);
            }
            else if (name == "R_foot")
            {
                maxoncmd.getHomingMethodR(*maxonMotor, &frame);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomeoffsetDistance(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomePosition(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                // maxoncmd.getCurrentThresholdR(*maxonMotor, &frame);
                // canManager.sendAndRecv(motor, frame);
            }
            else if (name == "L_foot")
            {
                maxoncmd.getHomingMethodR(*maxonMotor, &frame);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomeoffsetDistance(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomePosition(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                // maxoncmd.getCurrentThresholdR(*maxonMotor, &frame);
                // canManager.sendAndRecv(motor, frame);
            }
            else if (name == "maxonForTest")
            {
                maxoncmd.getHomingMethodTest(*maxonMotor, &frame);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomeoffsetDistance(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getHomePosition(*maxonMotor, &frame, 0);
                canManager.sendAndRecv(motor, frame);

                maxoncmd.getCurrentThresholdL(*maxonMotor, &frame);
                canManager.sendAndRecv(motor, frame);
            }
        }
    }
}

void DrumRobot::initializeFolder()
{
    string syncDir = "../magenta/sync";
    string recordDir = "../magenta/record";
    string outputMidDir = "../magenta/generated";
    string outputVelDir = "../magenta/velocity";
    string outputCode = "../include/magenta";

    func.clear_directory(syncDir);
    func.clear_directory(recordDir);
    func.clear_directory(outputMidDir);
    func.clear_directory(outputVelDir);
    func.clear_directory(outputCode);
}

bool DrumRobot::initializePos(const string &input)
{
    // set zero
    if (input == "o")
    {
        for (const auto &motorPair : motors)
        {
            if (shared_ptr<TMotor> tMotor = dynamic_pointer_cast<TMotor>(motorPair.second))
            {
                tservocmd.comm_can_set_origin(*tMotor, &tMotor->sendFrame, 0);
                canManager.sendMotorFrame(tMotor);
                tMotor->finalMotorPosition = 0.0;

                usleep(1000*100);    // 100ms

                // cout << "Tmotor [" << tMotor->myName << "] set Zero \n";
                // cout << "Current Motor Position : " << tMotor->motorPosition / M_PI * 180 << "deg\n";
            }
            else if (shared_ptr<MaxonMotor> maxonMotor = dynamic_pointer_cast<MaxonMotor>(motorPair.second))
            {
                maxonMotor->finalMotorPosition = 0.0;
            }
        }

        cout << "set zero and offset setting ~ ~ ~\n";
        sleep(2);   // Set Zero 명령이 확실히 실행된 후

        arduino.setHeadLED(Arduino::POWER_ON);  // led 로딩바 차는 모션 실행

        maxonMotorEnable();
        setMaxonMotorMode("CSP");

        state.main = Main::AddStance;
        flagObj.setAddStanceFlag(FlagClass::HOME); // 시작 자세는 Home 자세와 같음

        return true;
    }
    else
    {
        cout << "Invalid command or not allowed in current state!\n";
        return false;
    }
}

void DrumRobot::initializeDrumRobot()
{
    string input;
    pathManager.initPathManager();
    initializeMotors();
    initializeCanManager();
    motorSettingCmd(); // Maxon
    canManager.setSocketNonBlock();
    dxl.initialize();
    usbio.initUSBIO4761();
    func.openCSVFile();
    arduino.connect("/dev/ttyACM0");
    initializeFolder(); // Magenta 관련 폴더 초기화

    cout << "System Initialize Complete. Waiting for Brain...\n";
    cout << "[System] Socket Server Starting..."<< endl;

    agentSocket.start(); // --- TCP Socket 통신 시작 --- //
    while(true)
    {
        if (agentSocket.isConnected())
        {
            cout << "[System] Brain Connected!" << endl;
            initializePos("o");
            break;
        }
        usleep(1000);
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                                    Exit                                    */
////////////////////////////////////////////////////////////////////////////////

void DrumRobot::deactivateControlTask()
{
    struct can_frame frame;

    canManager.setSocketsTimeout(0, 500000);

    for (auto &motorPair : motors)
    {
        string name = motorPair.first;
        auto &motor = motorPair.second;

        // 타입에 따라 적절한 캐스팅과 초기화 수행
        if (shared_ptr<TMotor> tMotor = dynamic_pointer_cast<TMotor>(motor))
        {
            tservocmd.comm_can_set_cb(*tMotor, &tMotor->sendFrame, 0);
            canManager.sendMotorFrame(tMotor);
            cout << "Exiting for motor [" << name << "]" << endl;
        }
        else if (shared_ptr<MaxonMotor> maxonMotor = dynamic_pointer_cast<MaxonMotor>(motor))
        {
            maxoncmd.getQuickStop(*maxonMotor, &frame);
            canManager.txFrame(motor, frame);

            maxoncmd.getSync(&frame);
            canManager.txFrame(motor, frame);
            cout << "Exiting for motor [" << name << "]" << endl;
        }
    }

    agentSocket.stop(); // --- TCP Socket 통신 종료 --- //

    dxl.DXLTorqueOff();
    arduino.disconnect();
}

/////////////////////////////////////////////////////////////////////////////////
/*                           Maxon Motor Function                             */
/////////////////////////////////////////////////////////////////////////////////

void DrumRobot::maxonMotorEnable()
{
    struct can_frame frame;
    canManager.setSocketsTimeout(2, 0);

    // 제어 모드 설정
    for (const auto &motorPair : motors)
    {
        string name = motorPair.first;
        shared_ptr<GenericMotor> motor = motorPair.second;
        if (shared_ptr<MaxonMotor> maxonMotor = dynamic_pointer_cast<MaxonMotor>(motor))
        {
            if (canManager.isSilModeEnabled() && !maxonMotor->isConected)
            {
                continue;
            }

            maxoncmd.getHomeMode(*maxonMotor, &frame);
            canManager.txFrame(motor, frame);

            maxoncmd.getOperational(*maxonMotor, &frame);
            canManager.txFrame(motor, frame);

            usleep(100000);

            maxoncmd.getShutdown(*maxonMotor, &frame);
            canManager.txFrame(motor, frame);

            maxoncmd.getSync(&frame);
            canManager.txFrame(motor, frame);

            usleep(100000);
            
            maxoncmd.getEnable(*maxonMotor, &frame);
            canManager.txFrame(motor, frame);

            maxoncmd.getSync(&frame);
            canManager.txFrame(motor, frame);
            
            cout << "Maxon Enabled\n";

            usleep(100000);

            // maxoncmd.getStartHoming(*maxonMotor, &frame);
            // canManager.txFrame(motor, frame);

            // maxoncmd.getSync(&frame);
            // canManager.txFrame(motor, frame);

            // usleep(100000);
            
            // cout << "Maxon Enabled(2) \n";
        }
    }
}

void DrumRobot::setMaxonMotorMode(string targetMode)
{
    struct can_frame frame;
    canManager.setSocketsTimeout(0, 10000);
    for (const auto &motorPair : motors)
    {
        string name = motorPair.first;
        shared_ptr<GenericMotor> motor = motorPair.second;
        if (shared_ptr<MaxonMotor> maxonMotor = dynamic_pointer_cast<MaxonMotor>(motorPair.second))
        {
            if (canManager.isSilModeEnabled() && !maxonMotor->isConected)
            {
                continue;
            }

            if (targetMode == "CSV")    // Cyclic Sync Velocity Mode
            {
                maxoncmd.getCSVMode(*maxonMotor, &frame);
                canManager.txFrame(motor, frame);
            }
            else if (targetMode == "CST")   // Cyclic Sync Torque Mode
            {
                maxoncmd.getCSTMode(*maxonMotor, &frame);
                canManager.txFrame(motor, frame);
            }
            else if (targetMode == "HMM")   // Homming Mode
            {
                maxoncmd.getHomeMode(*maxonMotor, &frame);
                canManager.txFrame(motor, frame);
            }
            else if (targetMode == "CSP")   // Cyclic Sync Position Mode
            {
                maxoncmd.getCSPMode(*maxonMotor, &frame);
                canManager.txFrame(motor, frame);
            }

            // 모드 바꾸고 껐다 켜주기

            maxoncmd.getShutdown(*maxonMotor, &frame);
            canManager.txFrame(motor, frame);

            maxoncmd.getSync(&frame);
            canManager.txFrame(motor, frame);

            usleep(100);

            maxoncmd.getEnable(*maxonMotor, &frame);
            canManager.txFrame(motor, frame);

            maxoncmd.getSync(&frame);
            canManager.txFrame(motor, frame);

            usleep(100);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                                   THREAD                                   */
////////////////////////////////////////////////////////////////////////////////

void DrumRobot::stateMachine()
{
    sleep(1);   // read thead에서 clear 할 때까지 기다림

    while (state.main != Main::Shutdown)
    {
        switch (state.main.load())
        {
            case Main::Ideal:
            {
                idealStateRoutine();
                break;
            }
            case Main::AddStance:
            {
                runAddStanceProcess();
                break;
            }
            case Main::Play:
            {
                runPlayProcess();
                break;
            }
            case Main::Test:
            {
                testManager.SendTestProcess();  
                state.main = Main::Ideal;
                break;
            }
            case Main::Error:
            {
                state.main = Main::Shutdown;
                break;
            }
            case Main::Shutdown:
            {
                break;
            }
            case Main::Pause:
            {
                pauseStateRoutine();
                break;
            }
        }
    }

    arduino.setHeadLED(Arduino::POWER_OFF);  // LED 프로그램 종료시 점등 -> Chzzk

    // Exit
    if (usbio.useUSBIO)
    {
        usbio.exitUSBIO4761();
    }
    canManager.setSocketBlock();
    deactivateControlTask();
}

void DrumRobot::sendLoopForThread()
{
    sleep(2);   // read thead에서 clear / initial pos Path 생성 할 때까지 기다림

    static SilCommandPipeWriter silWriter; // SIL frame tick 전용 (DXL 블록과 공유)
    int cycleCounter = 0; // 주기 조절을 위한 변수 (Tmotor : 5ms, Maxon : 1ms)
    sendLoopPeriod = chrono::steady_clock::now();
    while (state.main != Main::Shutdown)
    {
        sendLoopPeriod += chrono::microseconds(1000);  // 주기 : 1msec
        
        map<string, bool> fixFlags; // 각 모터의 고정 상태 저장
        
        if (!canManager.setCANFrame(fixFlags, cycleCounter))
        {   
            lastErrorReason = "위험 궤적 감지! 미들웨어가 모터 보호를 위해 무리한 움직임을 차단했습니다."; // 모터 송신 에러
            state.main = Main::Error;
            break;
        }

        if (cycleCounter == 0) // 5ms마다 실행
        {
            // fixeFlags를 확인해서 1개라도 false면 무빙, 엘스 fixed
            bool isMoving = false;
            for (const auto &fixFlag : fixFlags)
            {
                if (!fixFlag.second)
                {
                    isMoving = true;
                    break;
                }
            }

            if (isMoving)
            {
                flagObj.setFixationFlag("moving");
            }
            else
            {
                flagObj.setFixationFlag("fixed");
            }
        }

        //////////////////////////////////////////////////////////////////////////
        ///////////////////////////////////보내기///////////////////////////////////
        //////////////////////////////////////////////////////////////////////////

        bool isWriteError = false; 
        
        for (auto &motor_pair : motors)
        {
            shared_ptr<GenericMotor> motor = motor_pair.second;
            // t모터는 5번 중 1번만 실행
            if (motor->isTMotor()) 
            {
                if (cycleCounter == 0) // 5ms마다 실행
                {
                    if (!canManager.sendMotorFrame(motor))
                    {
                        isWriteError = true;
                    }
                }
            }
            else if (motor->isMaxonMotor())
            {
                // Maxon모터는 1ms마다 실행
                if (!canManager.sendMotorFrame(motor))
                {
                    isWriteError = true;
                }
            }
        }

        int n = virtualMaxonMotor.size();
        for(int i = 0; i < n; i++)
        {
            maxoncmd.getSync(&virtualMaxonMotor[i]->sendFrame);

            if (!canManager.sendMotorFrame(virtualMaxonMotor[i]))
            {
                isWriteError = true;
            };
        }

        // DXL
        if (cycleCounter == 0)
        {
            silWriter.setEnabled(canManager.isSilModeEnabled());
            
            vector<vector<float>> dxlCommand;
            bool hasDxlCommand = false;
            {
                std::lock_guard<std::mutex> lock(pathManager.dxlBufferMutex);
                if (!pathManager.dxlCommandBuffer.empty())
                {
                    // 맨 앞 원소 꺼낸 값으로 SyncWrite 실행
                    dxlCommand = pathManager.dxlCommandBuffer.front();
                    pathManager.dxlCommandBuffer.pop();
                    hasDxlCommand = true;
                }
            }

            if (hasDxlCommand)
            {
                // 실제로 소비되는 DXL command를 head_pan/head_tilt 값으로 pipe에 내보낸다.
                if (dxlCommand.size() >= 2)
                {
                    if (dxlCommand[0].size() >= 3)
                    {
                        silWriter.writeDxl("head_pan", dxlCommand[0][2]);
                    }
                    if (dxlCommand[1].size() >= 3)
                    {
                        silWriter.writeDxl("head_tilt", dxlCommand[1][2]);
                    }
                }

                dxl.syncWrite(dxlCommand);

                map<int, float> pos_act = dxl.syncRead();
            }
        }

        // 1ms 주기마다 SIL frame tick 전송 (Python reader의 frame 경계 기준)
        silWriter.setEnabled(canManager.isSilModeEnabled());
        silWriter.writeTick();

        if (isWriteError)
        {
                        lastErrorReason = "모터 연결 끊김! CAN 통신 케이블이나 전원을 확인해 주세요."; // CAN 통신 에러
            state.main = Main::Error;
        }

        // 모든 모터가 연결 안된 경우 : 바로 fixed
        if(allMotorsUnConected)
        {
            flagObj.setFixationFlag("fixed");
        }

        cycleCounter = (cycleCounter + 1) % 5;

        this_thread::sleep_until(sendLoopPeriod);
    }
}

void DrumRobot::recvLoopForThread()
{
    canManager.clearReadBuffers();

    while (state.main != Main::Shutdown)
    {
        recvLoopPeriod = chrono::steady_clock::now();
        recvLoopPeriod += chrono::microseconds(100);  // 주기 : 100us

        canManager.readFramesFromAllSockets(); 
        bool isSafe = canManager.distributeFramesToMotors(true);
        if (!isSafe)
        {
            lastErrorReason = "물리적 한계 초과! 관절 각도가 제한 범위를 벗어났거나 과전류가 발생했습니다."; // 모터 수신 에러
            state.main = Main::Error;
        }
        
        this_thread::sleep_until(recvLoopPeriod);
    }
}

void DrumRobot::musicMachine()
{
    bool played = false;
    bool waiting = false;
    unique_ptr<sf::Music> music;

    while (state.main != Main::Shutdown)
    {
        if (state.main == Main::Play)
        {
            if (playMusic)
            {
                if (setWaitingTime)
                {
                    music = make_unique<sf::Music>();
                    if (!music->openFromFile(wavPath)) {
                        cerr << "음악 파일 열기 실패: " << wavPath << "\n";
                        music.reset(); // 파괴
                        continue;
                    }
                    cout << "음악 준비 완료. 동기화 타이밍 대기 중...\n";
                    setWaitingTime = false;
                    waiting = true;
                }

                if (waiting)
                {
                    // 재생
                    if (!played && chrono::system_clock::now() >= syncTime)
                    {
                        pathManager.startOfPlay = true;
                        music->play();
                        played = true;
                        cout << "음악 재생 시작 (동기화 완료)\n";
                    }

                    // 재생 종료
                    if (played && music->getStatus() != sf::Music::Playing)
                    {
                        cout << "음악 재생 완료\n";
                        played = false;
                        waiting = false;
                        music.reset();  // 안전하게 소멸
                    }
                }
            }
            else
            {
                if (setWaitingTime)
                {
                    setWaitingTime = false;
                    waiting = true;
                }

                if (waiting)
                {
                    if (chrono::system_clock::now() >= syncTime)
                    {
                        pathManager.startOfPlay = true;
                        waiting = false;
                    }
                }
            }
        }

        this_thread::sleep_for(chrono::milliseconds(1));
    }
}

void DrumRobot::runPythonInThread()
{
    int recordNum[100] = {0};
    int creationNum[100] = {0};

    while (state.main != Main::Shutdown)
    {
        if (runPython)
        {
            string pythonCmd = pythonScript + " " + pythonArgs;

            if (pythonArgs == "--sync")
            {
                pythonCmd += " --path ../magenta/ &";   // 경로 설정 & 백그라운드 실행

                int ret = system(pythonCmd.c_str());
                if (ret != 0)
                    cerr << "Python script failed to execute with code " << ret << endl;
                
                cout << "\nPython script is running... \n";
            }
            else if (pythonArgs == "--record")
            {
                pythonCmd += " --repeat " + to_string(repeatNum);

                pythonCmd += " --param";
                for (int i = 0; i < repeatNum; i++)
                {
                    int dT = delayTime.front(); delayTime.pop();
                    int rT = recordBarNum.front(); recordBarNum.pop();
                    int mT = makeBarNum.front(); makeBarNum.pop();

                    pythonCmd += " " + to_string(dT);
                    pythonCmd += " " + to_string(rT);
                    pythonCmd += " " + to_string(mT);

                    recordNum[i] = rT / 2;
                    creationNum[i] = mT / 2;
                }

                pythonCmd += " --bpm";
                pythonCmd += " " + to_string(pathManager.bpmOfScore);

                pythonCmd += " --path ../magenta/ &";   // 경로 설정 & 백그라운드 실행

                int ret = system(pythonCmd.c_str());
                if (ret != 0)
                    cerr << "Python script failed to execute with code " << ret << endl;

                cout << "\nPython script is running... \n";
                for (int i = 0; i < repeatNum; i++)
                {
                    for (int j = 0; j < creationNum[i]; j++)
                    {
                        string outputMid;
                        string outputVel = "null";
                        bool startFlag = (j == 0);
                        bool endFlag = (j == creationNum[i] - 1);
                        
                        // 디스이즈미용
                        outputMid = "../magenta/generated/output" + to_string(i) + "_" + to_string(j + 1) + "3.mid";
                        // if (j < recordNum[i])
                        // {
                        //     outputMid = "../magenta/generated/output" + to_string(i) + "_" + to_string(j + 1) + "3.mid";
                        // }
                        // else if (j < 2 * recordNum[i])
                        // {
                        //     outputMid = "../magenta/generated/output" + to_string(i) + "_" + to_string(j - recordNum[i] + 1) + "4.mid";
                        // }
                        // else
                        // {
                        //     outputMid = "../magenta/generated/output" + to_string(i) + "_" + to_string(j - 2 * recordNum[i] + 1) + "2.mid";
                        // }

                        // 해당 미디 파일 생성될 때까지 대기
                        while(!filesystem::exists(outputMid))
                            this_thread::sleep_for(chrono::milliseconds(1));

                        usleep(100*1000);
                        generateCodeFromMIDI(outputMid, outputVel, j, startFlag, endFlag);
                    }
                }

                // 폴더 비우기
                // string recordDir = "../magenta/record";
                // string outputMidDir = "../magenta/generated";
                // string outputVelDir = "../magenta/velocity";
            
                // func.clear_directory(recordDir);
                // func.clear_directory(outputMidDir);
                // func.clear_directory(outputVelDir);
            }

            runPython = false;
        }

        this_thread::sleep_for(chrono::milliseconds(1));
    }
}

string DrumRobot::makeStateJson()
{
    stringstream json_stream;
    int state_id = static_cast<int>(state.main.load());
    const string& song_code = nextSongCode;
    const string& last_cmd = lastExecutedCmd;
    const string& error_msg = lastErrorReason;
    bool lock_open = isLockKeyRemoved;
    int repeat_now = currentIterations;
    int repeat_max = repeatNum;
    const vector<string> joint_list = {
        "waist",
        "R_arm1",
        "L_arm1",
        "R_arm2",
        "R_arm3",
        "L_arm2",
        "L_arm3",
        "R_wrist",
        "L_wrist",
        "R_foot",
        "L_foot"
    };

    json_stream << "{";
    
    // 1. 기존 필수 데이터
    json_stream << "\"state\": " << state_id << ", ";
    json_stream << "\"bpm\": " << pathManager.bpmOfScore << ", ";
    json_stream << "\"is_fixed\": " << (flagObj.getFixationFlag() ? "true" : "false") << ", ";
    
    // 2. [추가] 곡명 및 진행률 (Context)
    json_stream << "\"current_song\": \"" << song_code << "\", ";
    json_stream << "\"progress\": \"" << repeat_now << "/" << repeat_max << "\", ";

    // 3. [추가] 잠금 키 해제 여부
    json_stream << "\"is_lock_key_removed\": " << (lock_open ? "true" : "false") << ", ";

    // 4. [추가] 마지막으로 실행된 명령어 및 에러 메시지 (Context)
    json_stream << "\"last_action\": \"" << last_cmd << "\", ";

    // 5. [추가] 현재 실측 관절 각도
    json_stream << "\"current_angles\": {";
    for (size_t joint_idx = 0; joint_idx < joint_list.size(); ++joint_idx) {
        const string& joint_name = joint_list[joint_idx];
        float curr_deg = 0.0f;

        if (motors.count(joint_name) > 0) {
            shared_ptr<GenericMotor> motor_ptr = motors.at(joint_name);
            if (motor_ptr) {
                curr_deg = (*motor_ptr).jointAngle * 180.0f / M_PI;
            }
        }

        // 허용 오차: 0.5도 (일반 관절), 1.0도 (손목/발목) - 이보다 차이가 크면 캐시 업데이트
        if (!has_joint_cache || joint_deg_cache.count(joint_name) == 0) {
            joint_deg_cache[joint_name] = curr_deg;
        } else {
            float prev_deg = joint_deg_cache[joint_name];
            float min_diff_deg = 0.5f;

            if (joint_name.find("wrist") != string::npos || joint_name.find("foot") != string::npos) {
                min_diff_deg = 1.0f;
            }

            if (fabs(curr_deg - prev_deg) >= min_diff_deg) {
                joint_deg_cache[joint_name] = curr_deg;
            }
        }

        json_stream << "\"" << joint_name << "\": " << fixed << setprecision(2) << joint_deg_cache[joint_name];
        if (joint_idx + 1 < joint_list.size()) {
            json_stream << ", ";
        }
    }
    json_stream << "}, ";
    
    // 6. [추가] 에러 상태일 때만 에러 메시지 포함, 아니면 "None"으로 표시
    if (state_id == 6) {
        json_stream << "\"error_message\": \"" << error_msg << "\"";
    } else {
        json_stream << "\"error_message\": \"None\"";
    }

    json_stream << "}";
    has_joint_cache = true;
    
    return json_stream.str();
}

void DrumRobot::broadcastStateThread() 
{
    string lastState = ""; // 마지막으로 전송한 상태 JSON 문자열
    
    // 로봇이 종료될 때까지 무한 반복
    while (state.main != Main::Shutdown) {
        
        if (agentSocket.isConnected()) {
            // 1. 현재 로봇의 상태를 JSON으로 조립
            string jsonState = makeStateJson();
        
            // 2. 상태가 변경되었을 경우에만 전송 (중복 방지)
            if (jsonState != lastState) {
                agentSocket.sendState(jsonState);
                lastState = jsonState;
            }
        }
        
        // 3. 100ms 대기 후 다시 전송
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}
/*
void DrumRobot::socketThread() {
    // ---------------------------------------------------------
    // 1. 소켓 설정 (server_test.cpp [1]~[2] 내용 복사)
    // ---------------------------------------------------------
    int PORT = 9999;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return;
    }

    // 포트 재사용 설정 (껐다 켰을 때 에러 방지)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        return;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return;
    }

    // 리슨 (대기)
    if (listen(server_fd, 3) < 0) {
        perror("Listen");
        return;
    }
    
    cout << ">>> [Socket] Server Thread Started on Port " << PORT << " <<<" << endl;

    // ---------------------------------------------------------
    // 2. 연결 수락 및 데이터 수신 루프 (server_test.cpp [3] 개조)
    // ---------------------------------------------------------
    
    // 로봇이 켜져있는 동안 계속 반복 (Main::Shutdown이 아닐 때)
    while (state.main != Main::Shutdown) { 
        
        cout << "[Socket] 클라이언트 연결 대기 중..." << endl;
        
        // accept()는 연결될 때까지 여기서 대기함 (Blocking)
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        if (new_socket < 0) {
            if (state.main == Main::Shutdown) break; // 종료 시 에러 무시
            perror("Accept failed");
            continue;
        }

        cout << "[Socket] 클라이언트 연결됨!" << endl;

        // 데이터 수신 루프
            char buffer[1024];
            while (state.main != Main::Shutdown) {
            memset(buffer, 0, 1024);
            
            // 데이터 읽기
            int valread = read(new_socket, buffer, 1024);
            
            if (valread <= 0) { // 연결 끊김
                cout << "[Socket] 클라이언트 연결 해제." << endl;
                close(new_socket);
                break; // 다시 accept 대기 상태로 돌아감
            }

            string cmd(buffer);
            
            // ★ 핵심: 받은 명령을 큐에 넣기 (Thread-Safe)
            {
                lock_guard<mutex> lock(queueMutex);
                commandQueue.push(cmd);
            }
            cout << "[Socket] 명령 수신 및 큐 저장: " << cmd << endl;
        }
    }
    
    // 종료 처리
    close(server_fd);
}
*/
////////////////////////////////////////////////////////////////////////////////
/*                                Ideal State                                 */
////////////////////////////////////////////////////////////////////////////////

void DrumRobot::displayAvailableCommands(string flagName) const
{
    cout << "Available Commands:\n";

    if (state.main == Main::Ideal)
    {
        if (flagName == "isHome")
        {
            cout << "- r : Move to Ready Pos\n";
            cout << "- t : Start Test\n";
            cout << "- s : Shut down the system\n";
            cout << "- u : Update Drum Position\n";
        }
        else if (flagName == "isReady")
        {
            cout << "- p : Play Drumming\n";
            cout << "- t : Start Test\n";
            cout << "- h : Move to Home Pos\n";
            // cout << "- m : Run Python (Magenta)\n";
        }
    }
    else
    {
        cout << "- s : Shut down the system\n";
    }
}

void DrumRobot::processInput(const string &input, string flagName)
{
     // 명령어 파싱 ("p:TIM" -> cmd="p", arg="TIM")
    string cmd = input;
    string arg = "";
    
    size_t delimiterPos = input.find(':');
    if (delimiterPos != string::npos) {
        cmd = input.substr(0, delimiterPos);
        arg = input.substr(delimiterPos + 1);
    }

    // -------------------------------------------------------
    // 1. Play (p) 명령 처리 - 자동화 로직 적용
    // -------------------------------------------------------
    if (cmd == "p") 
    {
        // 곡 이름이 함께 왔다면 설정 (예: p:TIM)
        if (!arg.empty()) {
            nextSongCode = arg; 
            cout << ">>> [Select] 곡 설정: " << arg << endl;
        }

        // [핵심] 현재 상태가 Ready가 아니라면 (예: Home 상태)
        if (flagName != "isReady") {
            cout << ">>> [Auto] 연주를 위해 Ready 자세로 먼저 이동합니다..." << endl;
            
            // 1) Ready 플래그 설정
            flagObj.setAddStanceFlag(FlagClass::READY);
            
            // 2) 자세 이동 실행 (이 함수는 모터가 다 움직일 때까지 대기함 - Source 164)
            runAddStanceProcess(); 
            
            // runAddStanceProcess 내부에서 state.main을 Ideal로 되돌리므로,
            // 여기서 다시 Play 흐름을 이어가야 합니다.
        }

        // 3) Ready 상태가 보장되었으므로 Play 모드 진입
        cout << ">>> [Action] 연주를 시작합니다." << endl;

        // [핵심] Play 시작 시점에 소켓 폐쇄 (명령 수신 중단)
        agentSocket.closeGate(); // 연주 시작 시 소켓 폐쇄
        cout << ">>> 수신 게이트가 폐쇄됩니다." << endl;

        state.main = Main::Play;
    }

    // -------------------------------------------------------
    // 2. Ready (r) 명령 처리
    // -------------------------------------------------------
    else if (cmd == "r")
    {
        // 이미 Ready 상태면 무시하거나 메시지 출력
        if (flagName == "isReady") {
            cout << ">>> 이미 준비된 상태입니다." << endl;
        } 
        else {
            flagObj.setAddStanceFlag(FlagClass::READY);
            state.main = Main::AddStance;
        }
    }

    // -------------------------------------------------------
    // 3. Home (h) 명령 처리
    // -------------------------------------------------------
    else if (cmd == "h")
    {
        // Play 중에는 h 명령이 안 먹히므로(Ideal 상태에서만 processInput 호출), 
        // Ready 상태에서만 Home으로 갈 수 있게 유지
        flagObj.setAddStanceFlag(FlagClass::HOME);
        state.main = Main::AddStance;
    }

    /*if (input == "r" && flagName == "isHome")
    {
        flagObj.setAddStanceFlag(FlagClass::READY);
        state.main = Main::AddStance;
    }
    else if (input == "p" && flagName == "isReady")
    {
        state.main = Main::Play;
    }
    // else if (input == "m" && flagName == "isReady")
    // {
        
    // }
    else if (input == "u" && flagName == "isHome")
    {
        pathManager.setDrumCoordinate();
        pathManager.setAddStanceAngle();

        cout << "\nUpdate Drum Position!!!\n";
        sleep(1);
    }
    else if (input == "h" && flagName == "isReady")
    {
        flagObj.setAddStanceFlag(FlagClass::HOME);
        state.main = Main::AddStance;
    }*/
   
    else if (cmd == "pause")
    {
        // Idle 상태에서 pause 요청 — 연주 중이 아니므로 무시
        cout << ">>> [Pause] 현재 연주 중이 아닙니다." << endl;
    }
    else if (cmd == "resume")
    {
        // Idle 상태에서 resume 요청 — 일시정지 상태가 아니므로 무시
        cout << ">>> [Resume] 현재 일시정지 상태가 아닙니다." << endl;
    }
    else if (input == "s" && flagName == "isHome")
    {
        flagObj.setAddStanceFlag(FlagClass::SHUTDOWN);
        state.main = Main::AddStance;
    }
    else if (input == "t")
    {
        state.main = Main::Test;
    }
    else
    {
        cout << "Invalid command or not allowed in current state!\n";
    }
}

void DrumRobot::idealStateRoutine()
{
    // 키 뽑았는지 확인하는 안전장치
    if (!isLockKeyRemoved)
    {
        // 안전한 상태인지 한 번만 실행됨
        string check;
        do
        {
            cout << ">>> 키 뽑았는지 확인 (k 입력): ";
            getline(cin, check);
        } while (check != "k");
        isLockKeyRemoved = true;
        agentSocket.openGate(); // 소켓으로 게이트 열었다고 신호 보내기
        cout << ">>> 키가 제거되었습니다. 수신 게이트가 활성화됩니다.\n"; 
    }

    // 로봇이 고정된 상태(Move가 끝난 상태)인지 확인 (Source 57 참조)
    if (flagObj.getFixationFlag())
    {
        string flag = flagObj.getAddStanceFlag();

        // 1. Shutdown 체크는 명령 수신 여부와 상관없이 가장 먼저!
        if (flag == "isShutDown")
        {
            state.main = Main::Shutdown;
            return;
        }

        // 2. 반복 재생 체크 (남은 횟수 있으면 명령 대기 없이 바로 출발)
        if (repeatNum > currentIterations) 
        {
            state.main = Main::Play;
            return;
        }

        // ========================================================
        // [수정 핵심] 큐(Queue) 확인으로 변경
        // ========================================================
        
        // 3. 명령 수신 (소켓에서 명령이 오면 popCommand로 꺼내옴)
        string input = agentSocket.popCommand();

        // 4. 명령 처리
        if (!input.empty())
        {
            std::vector<std::string> commands = agentAction.unpackCommands(input);

            for (const auto& command : commands)
            {
                if (command.empty()) {
                    continue;
                }

                lastExecutedCmd = command; // 마지막으로 실행된 명령어 저장

                if (handleModifier(command))
                {
                    continue;
                }

                // (A) 상태 제어 명령 (r, p, h, s, t, u) -> 기존 상태 머신(processInput)으로
                // p:TIM 처럼 콜론이 붙은 곡 선택 명령도 포함
                if (command == "r" || command == "h" || command == "s" || command == "t" || command == "u" ||
                    command == "pause" || command == "resume" ||
                    command.rfind("p", 0) == 0) // "p"로 시작하는 경우 (p 또는 p:TIM)
                {
                    processInput(command, flag);
                }
                // (B) 행동 제어 명령 (look, gesture, led, move) -> AgentAction으로 직행
                else 
                {
                    // state 변경 없이 즉시 행동 수행
                    agentAction.executeCommand(command);
                }
            }
        }
        // 5. [필수] CPU 폭주 방지 (1ms 대기)
        else
        {
            usleep(1000); 
        }
    }
    else
    {
        usleep(100);
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                            AddStance State                                 */
////////////////////////////////////////////////////////////////////////////////

void DrumRobot::runAddStanceProcess()
{
    string flag = flagObj.getAddStanceFlag();

    pathManager.genAndPushAddStance(flag);

    state.main = Main::Ideal;

    // send thread에서 읽기 전까지 대기
    while ((!allMotorsUnConected) && flagObj.getFixationFlag())
    {
       usleep(100); 
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                              Play State                                    */
////////////////////////////////////////////////////////////////////////////////

void DrumRobot::initializePlayState()
{
    measureMatrix.resize(1, 9);
    measureMatrix = MatrixXd::Zero(1, 9);

    endOfScore = false;
    measureTotalTime = 0.0;     ///< 악보 총 누적 시간. [s]

    pathManager.initPlayStateValue();
}

void DrumRobot::setSyncTime(int waitingTimeMillisecond)
{   
    // wait time + sync time 더해서 기다릴수 있도록 세팅 여기서 세팅된 시간이 pathManager 에서 기다리게 된다.   
    // 싱크 타임이 두개 거나 악보 읽는것도 두개거나
    // txt 파일 시간 읽어오기
    ifstream infile(syncPath);
    string time_str;
    if (!infile || !(infile >> time_str)) {
        cerr << "sync.txt 파일을 읽을 수 없습니다.\n" << syncPath;
        return;
    }

    // HH:MM:SS.mmm 파싱
    int hour, min, sec, millis;
    char sep1, sep2, dot;
    istringstream iss(time_str);
    if (!(iss >> hour >> sep1 >> min >> sep2 >> sec >> dot >> millis)) {
        cerr << "시간 형식 파싱 실패\n";
        return;
    }

    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);
    tm* local_tm = localtime(&tt);
    local_tm->tm_hour = hour;
    local_tm->tm_min = min;
    local_tm->tm_sec = sec;
    auto base_time = chrono::system_clock::from_time_t(mktime(local_tm)) + chrono::milliseconds(millis);

    remove(syncPath.c_str());      // syncTime 업데이트 하고 sync.txt 바로 지움
    
    syncTime = base_time + chrono::milliseconds(waitingTimeMillisecond);
    setWaitingTime = true;
}

void DrumRobot::displayPlayCommands(bool useMagenta, bool useDrumPad, float inputWaitMs, string txtFileName)
{
    int ret = system("clear");
    if (ret == -1) cout << "system clear error" << endl;

    // magenta or code name
    if(useMagenta)
    {
        cout << "magenta: On \n";
        cout << "trigger: Play When The Drum Pad is Struck. \n";
        cout << "args:\n";
        
        int argsSize = delayTime.size();
        if (repeatNum == argsSize)
        {
            cout << "\t- Repeat Num:" << repeatNum << "\n";
            for (int i = 0; i < repeatNum; i++)
            {
                float a, b, c, d;
                a = delayTime.front(); b = recordBarNum.front(); c = makeBarNum.front(); d = waitTime.front();
                delayTime.pop(); recordBarNum.pop(); makeBarNum.pop(); waitTime.pop();
                delayTime.push(a); recordBarNum.push(b); makeBarNum.push(c); waitTime.push(d);

                cout << "\t- " << i + 1 << "번째 Delay Time/Record Bar/Make Bar/Wait Time - (" << a << "/" << b << "/" << c << "/" << d << ") (s)\n";
            }
        }
        else
        {
            cout << "\t- Arguments Required\n";
        }
    }
    else
    {
        cout << "magenta: Off \n";
        cout << "code: " << txtFileName << "\n";

        if (useDrumPad)
        {
            cout << "trigger: Play When The Drum Pad is Struck (" << inputWaitMs/1000.0 << "s)\n";
        }
        else
        {
            cout << "trigger: Play After a Set Time Delay (" << inputWaitMs/1000.0 << "s)\n";
        }
    }

    // bpm
    cout << "bpm: " << pathManager.bpmOfScore << "\n";
    
    // maxon motor mode
    cout << "Maxon Motor Mode: ";
    if (pathManager.maxonMode == "unknown")
        cout << "unknown \n";
    else if (pathManager.maxonMode == "CST")
        cout << "CST mode \n";
    else
        cout << "CSP mode \n";
    // Tmotor mode
    cout << "Tmotor Mode: ";
    if (pathManager.tmotorMode == "unknown")
        cout << "unknown \n";
    // else if (pathManager.tmotorMode == "velocityFF")
    //     cout << "velocity control mode (only Feedforward) \n";
    // else if (pathManager.tmotorMode == "velocityFB")
    //     cout << "velocity control mode (only Feedback) \n";
    else if (pathManager.tmotorMode == "velocity")
        cout << "velocity control mode \n";
    else
        cout << "position control mode \n";
    
    // music
    if (playMusic)
    {
        cout << "music: Drumming With Music \n";
        cout << "\t- path:" << wavPath << "\n";
    }
    else
    {
        cout << "music: Just Drumming \n";
    }
    
    cout << "\nEnter Command (magenta, ";
    if (useMagenta)
        cout << "args, ";
    else
        cout << "code, trigger, ";
    cout << "bpm, modeM, modeT, music, run, exit): ";
}

void DrumRobot::setPythonArgs()
{
    cout << "\n반복 횟수: ";
    cin >> repeatNum;

    // 큐 초기화 (모든 요소 삭제)
    while (!delayTime.empty()) {
        delayTime.pop();  // 큐에서 항목을 하나씩 제거
    }
    while (!recordBarNum.empty()) {
        recordBarNum.pop();
    }
    while (!makeBarNum.empty()) {
        makeBarNum.pop();
    }
    while (!waitTime.empty()) {
        waitTime.pop();
    }

    // 반복횟수에 따라 딜레이, 녹음, 생성 시간 입력
    for(int i = 0; i < repeatNum; i++)
    {
        int dT, rT, mT;
        float wT;
        
        cout << "\n" << i + 1 << "번째 delay time: ";
        cin >> dT;
        
        do{ 
            cout << i + 1 << "번째 record bar number: ";
            cin >> rT;
            if (rT % 2 == 1)
                cout << "녹음 마디 갯수는 2의 배수여야 합니다.\n";
        }while(rT % 2 == 1);
        
        do{
            cout << i + 1 << "번째 make bar number: ";
            cin >> mT;
            if(mT % 2 == 1)
                cout << "생성 마디 갯수는 2의 배수여야 합니다.\n";
            if(mT > 3*rT)
                cout << "생성 마디 갯수는 녹음 마디의 3배 이하여하 합니다.\n";
        }while(mT % 2 == 1 || mT > 3*rT);

        cout << i + 1 << "번째 wait time: ";
        cin >> wT;

        delayTime.push(dT);
        recordBarNum.push(rT);
        makeBarNum.push(mT);
        waitTime.push(wT);
    }
}

bool DrumRobot::checkPreconditions(bool useMagenta, string txtPath)
{
    if (useMagenta)
    {
        // args 확인
        int argsSize = delayTime.size();
        if ((repeatNum != argsSize))
        {
            return false;
        }

        // mode 확인
        if (pathManager.maxonMode == "unknown" || pathManager.tmotorMode == "unknown")
        {
            return false;
        }
    }
    else
    {
        // code 확인
        if (txtPath == "null")
        {
            return false;
        }

        // mode 확인
        if (pathManager.maxonMode == "unknown" || pathManager.tmotorMode == "unknown")
        {
            return false;
        }
    }

    return true;
}

string DrumRobot::selectPlayMode()
{
    string userInput;
    int cnt = 0;    // 입력 횟수 (일정 횟수 초과되면 오류)
    const int maxAttempts = 999;    // 최대 시도 횟수

    string errCode = "null"; // Ideal 로 이동

    bool useMagenta = false;
    bool useDrumPad = false;
    int mode = 1;
    int triggerMode = 1;
    float inputWaitMs = 3000.0; // 3s
    static string txtFileName = "null";
    string txtPath = txtBaseFolderPath + txtFileName;;
    string wavFileName = "null";

    while(cnt < maxAttempts)
    {
        displayPlayCommands(useMagenta, useDrumPad, inputWaitMs, txtFileName);
        cin >> userInput;

        if (userInput == "magenta")
        {
            if (useMagenta)
                useMagenta = false;
            else
                useMagenta = true;
        }
        else if (userInput == "args" && useMagenta)
        {
            txtPath = magentaCodePath;    // 마젠타 사용 시 최종 출력 파일 이름
            
            setPythonArgs();
        }
        else if (userInput == "code" && (!useMagenta))
        {
            cout << "\nEnter Music Code Name: ";
            cin >> txtFileName;

            txtPath = txtBaseFolderPath + txtFileName;
            repeatNum = 1;
        }
        else if (userInput == "trigger" && (!useMagenta))
        {
            cout << "\nEnter Trigger Mode (Play After a Set Time Delay : 1 / Play When The Drum Pad is Struck : 0): ";
            cin >> triggerMode;
            cout << "Enter Waiting Time: ";
            cin >> inputWaitMs;
            inputWaitMs *= 1000;

            if (triggerMode == 1)
                useDrumPad = false;
            else if (triggerMode == 0)
                useDrumPad = true;
        }
        else if (userInput == "bpm")
        {
            do
            {
                cout << "\nEnter Initial BPM of Music: ";
                cin >> pathManager.bpmOfScore;

                if (pathManager.bpmOfScore <= 0)
                {
                    cout << "\nInvalid Input (BPM <= 0)\n";
                }

            } while (pathManager.bpmOfScore <= 0);
        }
        else if (userInput == "modeM")
        {
            do
            {
                cout << "\nEnter Maxon Control Mode (CSP: 1 / CST: 0): ";
                cin >> mode;

                if (mode == 0)
                {
                    pathManager.maxonMode = "CST";

                    // Kp 값 입력받기
                    cout << "Kp: ";
                    cin >> pathManager.Kp;

                    // Kd 값 입력받기
                    cout << "Kd: ";
                    cin >> pathManager.Kd;

                    // Kd 값 입력받기
                    cout << "KdDrop: ";
                    cin >> pathManager.KdDrop;

                    // KpMin 값 입력받기
                    cout << "KpMin (0~1): ";
                    cin >> pathManager.kpMin;

                    // KpMax 값 입력받기
                    cout << "KpMax: ";
                    cin >> pathManager.kpMax;
                }
                else if (mode == 1)
                {
                    pathManager.maxonMode = "CSP";
                }
                else
                {
                    cout << "\nInvalid Input\n";
                    pathManager.maxonMode = "unknown";
                }
            } while (pathManager.maxonMode == "unknown");
        }
        else if (userInput == "modeT")
        {
            do
            {
                cout << "\nEnter Tmotor Control Mode (position: 1 / velocity: 2): ";
                cin >> mode;

                if (mode == 1)
                {
                    pathManager.tmotorMode = "position";
                }
                else if (mode == 2)
                {
                    pathManager.tmotorMode = "velocity";
                }
                // else if (mode == 3)
                // {
                //     pathManager.tmotorMode = "velocityFB";
                // }
                // else if (mode == 4)
                // {
                //     pathManager.tmotorMode = "velocityFF";
                // }
                else
                {
                    cout << "\nInvalid Input\n";
                    pathManager.tmotorMode = "unknown";
                }
            } while (pathManager.tmotorMode == "unknown");
        }
        else if (userInput == "music")
        {
            if (playMusic)
            {
                playMusic = false;
            }
            else
            {
                cout << "\nEnter Music Name: ";
                cin >> wavFileName;
                wavPath = wavBaseFolderPath + wavFileName + ".wav";
                playMusic = true;
            }
        }
        else if (userInput == "run")
        {
            if (checkPreconditions(useMagenta, txtPath))   // 오류 사전 점검
            {
                if (useMagenta)
                {
                    pythonArgs = "--record";
                    runPython = true;

                    string txtIndexPath = txtPath + "0.txt";
                    while (!filesystem::exists(txtIndexPath)) {
                        this_thread::sleep_for(chrono::milliseconds(1)); // 1ms 대기
                    }

                    inputWaitMs = waitTime.front() * 1000;
                    waitTime.pop();
                    setSyncTime((int)inputWaitMs);
                }
                else if (useDrumPad)
                {
                    // ★★★ [여기 추가] 출발 전 기존 파일 삭제 (좀비 파일 제거) ★★★
                    //remove(syncPath.c_str());
                    pythonArgs = "--sync";
                    runPython = true;

                    while (!filesystem::exists(syncPath)) {
                        this_thread::sleep_for(chrono::milliseconds(1)); // 1ms 대기
                    }

                    setSyncTime((int)inputWaitMs);
                }
                else
                {
                    syncTime = chrono::system_clock::now() + chrono::milliseconds((int)inputWaitMs);
                    setWaitingTime = true;
                }
                
                return txtPath;
            }
            else
            {
                cout << "\nInvalid Command\n";
                sleep(1);
            }
        }
        else if (userInput == "exit")
        {
            return errCode;
        }
        else
        {
            cout << "\nInvalid Command\n";
            sleep(1);
        }

        cnt++;
    }

    cout << "\n입력을 시도한 횟수가 " << maxAttempts << " 이상입니다\n";
    sleep(1);

    return errCode;
}

string DrumRobot::trimWhitespace(const string &str)
{
    size_t first = str.find_first_not_of(" \t"); // 문자열의 시작에서 공백과 탭을 건너뛴 첫 번째 위치를 찾음
    // 만약 문자열이 모두 공백과 탭으로 이루어져 있다면, 첫 번째 위치는 npos가 됨
    if (string::npos == first)
    {
        return str;
    }
    size_t last = str.find_last_not_of(" \t"); // 문자열의 끝에서 공백과 탭을 건너뛴 마지막 위치를 찾음
    return str.substr(first, (last - first + 1));
}

bool DrumRobot::handleModifier(const string &command)
{
    size_t delimiter_pos = command.find(':');
    if (delimiter_pos == string::npos)
    {
        return false;
    }

    string key = command.substr(0, delimiter_pos);
    string value_text = command.substr(delimiter_pos + 1);

    try
    {
        if (key == "tempo_scale")
        {
            double scale = stod(value_text);
            if (scale <= 0.0)
            {
                cout << ">>> [Modifier] tempo_scale는 0보다 커야 합니다: " << value_text << endl;
                return true;
            }

            pending_modifier.tempo_scale = scale;

            ostringstream scale_text;
            scale_text << fixed << setprecision(2) << scale;
            cout << ">>> [Modifier] tempo_scale 설정: " << scale_text.str() << endl;
            return true;
        }

        if (key == "velocity_delta")
        {
            int delta = stoi(value_text);
            pending_modifier.velocity_delta = delta;
            cout << ">>> [Modifier] velocity_delta 설정: " << delta << endl;
            return true;
        }
    }
    catch (const exception &error)
    {
        cout << ">>> [Modifier] 파싱 실패: " << command << " (" << error.what() << ")" << endl;
        return true;
    }

    return false;
}

bool DrumRobot::hasPlayModifier(const PlayModifier &modifier) const
{
    if (fabs(modifier.tempo_scale - 1.0) > 0.0001)
    {
        return true;
    }

    return modifier.velocity_delta != 0;
}

double DrumRobot::applyTempoScale(double bpm) const
{
    if (active_modifier.tempo_scale <= 0.0)
    {
        return bpm;
    }

    return bpm * active_modifier.tempo_scale;
}

double DrumRobot::applyVelocityDelta(double value) const
{
    if (value <= 0.0)
    {
        return 0.0;
    }

    double next_value = value + static_cast<double>(active_modifier.velocity_delta);

    if (next_value < 1.0)
    {
        next_value = 1.0;
    }

    // 현재 코드 악보는 0~8 강세 스케일을 사용하므로 범위를 유지한다.
    if (next_value > 8.0)
    {
        next_value = 8.0;
    }

    return next_value;
}

bool DrumRobot::readMeasure(ifstream& inputFile)
{
    // 이인우: stod 예외처리 해줘야 함

    string row;
    double timeSum = 0.0;

    for (int i = 1; i < measureMatrix.rows(); i++)
    {
        timeSum += measureMatrix(i, 1);
    }

    // timeSum이 threshold를 넘으면 true 반환
    if (timeSum > measureThreshold)
    {
        return true;
    }

    while (getline(inputFile, row))
    {
        istringstream iss(row);
        string item;
        vector<string> items;

        while (getline(iss, item, '\t'))
        {
            item = trimWhitespace(item);
            items.push_back(item);
        }

        if (items[0] == "bpm")                          // bpm 변경 코드
        {
            // cout << "\n bpm : " << pathManager.bpmOfScore;
            double score_bpm = stod(items[1]);
            pathManager.bpmOfScore = applyTempoScale(score_bpm);
            // cout << " -> " << pathManager.bpmOfScore << "\n";
        }
        else if (items[0] == "end")                     // 종료 코드
        {
            endOfScore = true;
            return false;
        }
        else if (stod(items[0]) < 0)                     // 종료 코드 (마디 번호가 음수)
        {
            endOfScore = true;
            return false;
        }
        else
        {
            measureMatrix.conservativeResize(measureMatrix.rows() + 1, measureMatrix.cols());
            for (int i = 0; i < 8; i++)
            {
                double cell_value = stod(items[i]);
                if (i == 4 || i == 5)
                {
                    cell_value = applyVelocityDelta(cell_value);
                }

                measureMatrix(measureMatrix.rows() - 1, i) = cell_value;
            }

            // total time 누적
            measureTotalTime += measureMatrix(measureMatrix.rows() - 1, 1) * 100.0 / pathManager.bpmOfScore;
            measureMatrix(measureMatrix.rows() - 1, 8) = measureTotalTime;

            // timeSum 누적
            timeSum += measureMatrix(measureMatrix.rows() - 1, 1);

            // timeSum이 threshold를 넘으면 true 반환
            if (timeSum > measureThreshold)
            {
                return true;
            }
        }
    }

    // // 루프가 끝난 후, 실패 원인 분석
    // if (inputFile.eof()) {
    //     cout << "getline()이 파일 끝(EOF)에 도달하여 종료되었습니다." << endl;
    // }
    // else if (inputFile.fail()) {
    //     cout << "getline()이 논리적 오류로 실패했습니다." << endl;
    // }
    // else if (inputFile.bad()) {
    //     cout << "getline()이 심각한 I/O 오류로 실패했습니다." << endl;
    // }
    // cout << measureMatrix;
    return false;
}

void DrumRobot::checkPlayInterrupts()
{
    string input = agentSocket.popCommand();
    if (input.empty()) return;

    vector<string> commands = agentAction.unpackCommands(input);
    for (const string& command : commands)
    {
        if (command == "pause")
        {
            cout << ">>> [Play] 일시정지 요청." << endl;
            pause_requested = true;
        }
        else if (handleModifier(command))
        {
            // 다음 마디부터 즉시 반영
            active_modifier = pending_modifier;
            pending_modifier = PlayModifier();
            cout << ">>> [Play] modifier 즉시 적용." << endl;
        }
    }
}

void DrumRobot::pauseStateRoutine()
{
    cout << ">>> [Pause] 일시정지 상태. 'resume' 또는 'h' 명령을 기다립니다." << endl;
    arduino.setHeadLED(Arduino::IDLE);
    agentSocket.openGate(); // Pause 중에는 look/gesture/modifier 등 명령 수신 허용

    while (state.main == Main::Pause)
    {
        string input = agentSocket.popCommand();

        if (!input.empty())
        {
            vector<string> commands = agentAction.unpackCommands(input);
            for (const string& command : commands)
            {
                if (command == "resume")
                {
                    cout << ">>> [Resume] 연주를 재개합니다. (파일 인덱스: " << play_file_index << ")" << endl;
                    agentSocket.closeGate(); // 연주 재개 시 gate 다시 닫기
                    is_resuming = true;
                    state.main = Main::Play;
                    return;
                }
                else if (command == "h")
                {
                    cout << ">>> [Pause->Home] 홈으로 복귀합니다." << endl;
                    play_file_index = 0;
                    is_resuming = false;
                    endOfScore = false;
                    flagObj.setAddStanceFlag(FlagClass::HOME);
                    agentSocket.openGate();
                    state.main = Main::AddStance;
                    return;
                }
                else if (handleModifier(command))
                {
                    // Pause 중 modifier 즉시 적용 — resume 후 readMeasure에서 반영됨
                    active_modifier = pending_modifier;
                    pending_modifier = PlayModifier();
                    cout << ">>> [Pause] modifier 적용. 재개 시 반영됩니다." << endl;
                }
                else
                {
                    // look/gesture/move 등 액션 명령 처리
                    agentAction.executeCommand(command);
                }
            }
        }

        usleep(10000); // 10ms
    }
}

void DrumRobot::runPlayProcess()
{
    string txtPath;
    string txtIndexPath;

    if (is_resuming)
    {
        // -------------------------------------------------------
        // Resume 경로: 저장된 play_file_index에서 재개
        // -------------------------------------------------------
        is_resuming = false;
        endOfScore = false;
        measureTotalTime = 0.0;
        measureMatrix.resize(1, 9);
        measureMatrix = MatrixXd::Zero(1, 9);
        txtPath = txtBaseFolderPath + nextSongCode;
        pathManager.startOfPlay = true;
        pathManager.endOfPlayCommand = false; // [수정] Resume 시 초기화 누락 방지
        arduino.setHeadLED(Arduino::PLAYING);
        cout << ">>> [Resume] '" << nextSongCode << "' 파일 인덱스 "
             << play_file_index << " 에서 재개합니다." << endl;
    }
    else
    {
        // -------------------------------------------------------
        // Fresh start 경로: 처음부터 연주
        // -------------------------------------------------------
        play_file_index = 0;

        // 1. 초기화
        initializePlayState();

        // =========================================================================
        // [수정] Agent 모드: selectPlayMode(키보드 입력) 제거 -> 변수 기반 자동 설정
        // =========================================================================

        // (1) 곡 이름 확인 (processInput에서 p:TIM 등으로 설정된 값)
        if (nextSongCode.empty()) {
            nextSongCode = "test_one"; // 기본값 설정 (안전장치)
        }

        cout << ">>> [Agent] Auto-Start Playing: " << nextSongCode << endl;

        // (2) 파일 경로 완성 (예: ../include/codes/TIM)
        txtPath = txtBaseFolderPath + nextSongCode; // 경로 완성

        // (3) 파일 존재 여부 확인 (TIM0.txt가 있는지 체크)
        string checkPath = txtPath + "0.txt";
        if (!filesystem::exists(checkPath))
        {
            cout << ">>> [Error] 악보 파일을 찾을 수 없습니다: " << checkPath << endl;
            cout << ">>> 대기 모드(Home)로 복귀합니다." << endl;
            state.main = Main::Ideal;
            flagObj.setAddStanceFlag(FlagClass::HOME);
            return;
        }

        active_modifier = pending_modifier;
        pending_modifier = PlayModifier();

        if (hasPlayModifier(active_modifier))
        {
            ostringstream modifier_text;
            modifier_text << fixed << setprecision(2) << active_modifier.tempo_scale;
            cout << ">>> [Modifier] 이번 연주 적용: tempo_scale=" << modifier_text.str()
                 << ", velocity_delta=" << active_modifier.velocity_delta << endl;
        }

        // (4) 연주 설정 강제 주입
        bool useMagenta = false;
        repeatNum = 1;
        currentIterations = 1;

        // LED 켜기
        arduino.setHeadLED(Arduino::PLAYING);

        // 1. BPM 기본값 설정
        if (pathManager.bpmOfScore <= 0) {
            pathManager.bpmOfScore = 100.0;
        }
        pathManager.bpmOfScore = applyTempoScale(pathManager.bpmOfScore);

    // 2. 모터 제어 모드 확정 (CSP: 위치 제어)
    // (InitializePos에서 이미 했지만, 안전을 위해 확실히 고정)
    if (pathManager.maxonMode == "unknown" || pathManager.maxonMode.empty()) {
        pathManager.maxonMode = "CSP"; 
    }
    if (pathManager.tmotorMode == "unknown" || pathManager.tmotorMode.empty()) {
        pathManager.tmotorMode = "position";
    }
    
        // Maxon 모터 게인(Gain) 값 설정
        pathManager.Kp = 300.0;
        pathManager.Kd = 30.0;
        pathManager.KdDrop = 30.0;
        pathManager.kpMin = 1;
        pathManager.kpMax = 300.0;

        cout << ">>> [System] 연주 시작 트리거 (Start)" << endl;
        pathManager.startOfPlay = true;
    } // end of fresh-start block

    // =================================================================
    while (!endOfScore)
    {
        // 연주 중 인터럽트(pause/modifier) 처리
        checkPlayInterrupts();

        if (pause_requested.load())
        {
            pause_requested = false;
            cout << ">>> [Play] 일시정지. 저장 파일 인덱스: " << play_file_index << endl;
            state.main = Main::Pause;
            return; // gate는 닫힌 채 유지 — pauseStateRoutine에서 처리
        }

        ifstream inputFile;
        txtIndexPath = txtPath + to_string(play_file_index) + ".txt";
        inputFile.open(txtIndexPath); // 파일 열기

        if (inputFile.is_open())     //////////////////////////////////////// 파일 열기 성공
        {
            if (inputFile.peek() == ifstream::traits_type::eof())
            {
                cout << "\n - The file exists, but it is empty.\n";
                inputFile.close();          // 파일 닫기
                usleep(100);                // 대기 : 악보 작성 중
            }
            else
            {
                bool mid_file_pause = false;
                while(readMeasure(inputFile))    // 한마디 분량 미만으로 남을 때까지 궤적/명령 생성
                {
                    checkPlayInterrupts();
                    if (pause_requested.load())
                    {
                        pause_requested = false;
                        mid_file_pause = true;
                        cout << ">>> [Play] 일시정지 요청. 즉시 정지합니다." << endl;
                        pathManager.clearCommandBuffers();
                        break;
                    }

                    pathManager.processLine(measureMatrix);

                    if (state.main == Main::Error) {
                        lastErrorReason = "자세 계산 실패! 역기구학(IK) 계산 결과 팔이 닿지 않거나 꼬이는 궤적입니다.";
                        return;
                    }
                }

                if (mid_file_pause)
                {
                    inputFile.close();
                    cout << ">>> [Play] 일시정지. 저장 파일 인덱스: " << play_file_index << endl;
                    state.main = Main::Pause;
                    return;
                }

                // 마지막 파일이면 잔여 명령도 여기서 생성
                if (endOfScore)
                {
                    while (!pathManager.endOfPlayCommand)
                    {
                        checkPlayInterrupts();
                        if (pause_requested.load())
                        {
                            pause_requested = false;
                            mid_file_pause = true;
                            cout << ">>> [Play] 일시정지 요청. 즉시 정지합니다." << endl;
                            pathManager.clearCommandBuffers();
                            break;
                        }

                        pathManager.processLine(measureMatrix);
                        if (state.main == Main::Error) {
                            lastErrorReason = "자세 계산 실패! 역기구학(IK) 계산 결과 팔이 닿지 않거나 꼬이는 궤적입니다.";
                            return;
                        }
                    }
                }

                if (mid_file_pause)
                {
                    inputFile.close();
                    cout << ">>> [Play] 일시정지. 저장 파일 인덱스: " << play_file_index << endl;
                    state.main = Main::Pause;
                    return;
                }

                inputFile.close(); // 파일 닫기

                // 파일 단위 실행 완료 대기 — 버퍼가 소진될 때까지 기다린 뒤 pause 여부 판단
                bool file_pause = false;
                while (!flagObj.getFixationFlag() && !allMotorsUnConected)
                {
                    checkPlayInterrupts();
                    if (pause_requested.load())
                    {
                        pause_requested = false;
                        file_pause = true;
                        cout << ">>> [Play] 일시정지 요청. 즉시 정지합니다." << endl;
                        pathManager.clearCommandBuffers();
                        break;
                    }
                    usleep(1000);
                }

                if (file_pause)
                {
                    cout << ">>> [Play] 일시정지. 저장 파일 인덱스: " << play_file_index << endl;
                    state.main = Main::Pause;
                    return;
                }

                play_file_index++; // 다음 파일 열 준비

                if (endOfScore)
                    break; // 마지막 파일 실행 완료 → 정상 종료
            }
        }
        else     //////////////////////////////////////////////////////////// 파일 열기 실패
        {
            if (play_file_index == 0)                        ////////// 1. Play 시작도 못한 경우 -> Ideal 로 이동
            {
                cout << "not find " << txtIndexPath << "\n";
                sleep(1);
                repeatNum = 1;
                currentIterations = 1;
                state.main = Main::Ideal;
                return;
            }
            else if (flagObj.getFixationFlag() && (!allMotorsUnConected))   ////////// 2. 명령 소진 -> 에러
            {
                cout << "Error : not find " << txtIndexPath << "\n";
                state.main = Main::Error;
                return;
            }
            else                                             ////////// 3. 다음 악보 생성될 때까지 대기
            {
                inputFile.clear();
                usleep(100);
            }
        }
    }

    if(txtPath == magentaCodePath)
    {
        // 악보 파일 저장 후 삭제
        for (int i = 0; i < play_file_index; i++)
        {
            txtIndexPath = txtPath + to_string(i) + ".txt";

            // 현재 시간 가져오기
            auto now = chrono::system_clock::now();
            time_t t = chrono::system_clock::to_time_t(now);
            tm localTime = *localtime(&t);

            // 시간 문자열 생성 (MMDDHHMM)
            ::ostringstream timeStream;
            timeStream << ::setw(2) << setfill('0') << localTime.tm_mon + 1   // 월
                    << setw(2) << setfill('0') << localTime.tm_mday       // 일
                    << setw(2) << setfill('0') << localTime.tm_hour       // 시
                    << setw(2) << setfill('0') << localTime.tm_min;       // 분
            string timeStr = timeStream.str();
            
            string saveFolder = "../../DrumRobot_data/codes/";

            string saveCode = saveFolder + "save_code_" + timeStr + "_" + to_string(currentIterations-1) + to_string(i+1) + ".txt";

            filesystem::rename(txtIndexPath.c_str(), saveCode.c_str());
            remove(txtIndexPath.c_str());
        }
    }

    cout << "Play is Over\n";
    play_file_index = 0; // 재사용에 대비해 리셋
    if (repeatNum == currentIterations)
    {
        flagObj.setAddStanceFlag(FlagClass::HOME); // 연주 종료 후 Home 으로 이동
        repeatNum = 1;
        currentIterations = 1;
    }
    else
    {
        flagObj.setAddStanceFlag(FlagClass::READY); // Play 반복 시 Ready 으로 이동
    }
    
    state.main = Main::AddStance;

    // 게이트 개방
    agentSocket.openGate();
    cout << " [System] 연주 종료. 명령 수신 게이트 개방." << endl;
}

////////////////////////////////////////////////////////////////////////////////
/*                                                                            */
////////////////////////////////////////////////////////////////////////////////

void DrumRobot::generateCodeFromMIDI(string midPath, string veloPath, int recordingIndex, bool startFlag, bool endFlag)
{
    // 경로 설정
    filesystem::path outputPath1 = "../include/magenta/output1_drum_hits_time.csv"; 
    filesystem::path outputPath2 = "../include/magenta/output2_mc.csv";   
    filesystem::path outputPath3 = "../include/magenta/output3_mc2c.csv";    
    filesystem::path outputPath4 = "../include/magenta/output4_hand_assign.csv";

    filesystem::path outputPath5 = "../include/magenta/output5_vel.txt";
    filesystem::path outputPath6 = "../include/magenta/output6_add_groove.txt";

    filesystem::path outputPath = magentaCodePath + to_string(recordingIndex) + ".txt";

    string outputVel = "../include/magenta/vel_output.txt";

    // mid 파일 받아서 악보 생성하기
    size_t pos;
    unsigned char runningStatus;
    // int initial_setting_flag = 0;
    double note_on_time = 0;

    vector<unsigned char> midiData;

    if (filesystem::exists(midPath) && flagObj.getAddStanceFlag() == "isReady")
    {
        if (!func.readMidiFile(midPath, midiData)) cout << "mid file error\n";
    } 
    // if (!func.readMidiFile(targetPath, midiData)) cout << "mid file error\n";

    pos = 14;
    int tpqn = (midiData[12] << 8) | midiData[13];
    int bpm;

    while (pos + 8 <= midiData.size()) {
        if (!(midiData[pos] == 'M' && midiData[pos+1] == 'T' && midiData[pos+2] == 'r' && midiData[pos+3] == 'k')) {
            // cerr << "MTrk expected at pos " << pos << "\n";
            break;
        }
        size_t trackLength = (midiData[pos+4] << 24) |
                        (midiData[pos+5] << 16) |
                        (midiData[pos+6] << 8) |
                        midiData[pos+7];
        pos += 8;
        size_t trackEnd = pos + trackLength;

        note_on_time = 0;
        while (pos < trackEnd) {
            size_t delta = func.readTime(midiData, pos);
            note_on_time += delta;
            func.analyzeMidiEvent(midiData, pos, runningStatus, note_on_time, tpqn, bpm, outputPath1);
        }
        pos = trackEnd;
    }

    //이거 세기 반영 시키는 변수 안하면 원본 그대로 
    bool mapTo357 = false;
    vector<Functions::Seg> segs;

    func.roundDurationsToStep(bpm, outputPath1, outputPath2); 
    func.convertMcToC(outputPath2, outputPath3);
    func.assignHandsToEvents(outputPath3, outputPath4);

    if (veloPath != "null")
    {
        // veloPath 세기 파일 outputFile 우리가 쓸 아웃풋 파일
        func.analyzeVelocityWithLowPassFilter(veloPath, outputVel, bpm);

        // 위에서 만든 아웃풋 파일 넣어주기 그럼 segs 에 필터씌운 정보 저장댐
        func.loadSegments(outputVel, segs);

        // 수정전 악보 scoreIn 최종 출력 파일 scoreOut
        func.applyIntensityToScore(segs, outputPath4, outputPath5, mapTo357);

        // 그루브 추가 
        func.addGroove(bpm, outputPath5, outputPath6);
        
        func.convertToMeasureFile(outputPath6, outputPath, startFlag, endFlag);

        remove(outputPath1.c_str());      // 중간 단계 txt 파일 삭제
        remove(outputPath2.c_str());
        remove(outputPath3.c_str());
        remove(outputPath4.c_str());

        remove(outputPath5.c_str());
        remove(outputPath6.c_str());
        remove(outputVel.c_str());
    }
    else
    {
        func.convertToMeasureFile(outputPath4, outputPath, startFlag, endFlag);

        remove(outputPath1.c_str());      // 중간 단계 txt 파일 삭제
        remove(outputPath2.c_str());
        remove(outputPath3.c_str());
        remove(outputPath4.c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////
/*                                  Flag                                      */
////////////////////////////////////////////////////////////////////////////////

FlagClass::FlagClass()
{

}

FlagClass::~FlagClass()
{

}

void FlagClass::setAddStanceFlag(AddStanceFlag flag)
{
    addStanceFlag = flag;
}

string FlagClass::getAddStanceFlag()
{
    if (addStanceFlag == HOME)
    {
        return "isHome";
    }
    else if (addStanceFlag == READY)
    {
        return "isReady";
    }
    else if (addStanceFlag == SHUTDOWN)
    {
        return "isShutDown";
    }

    return "isError";
}

void FlagClass::setFixationFlag(string flagName)
{
    if (flagName == "moving")
    {
        isFixed = false;
    }
    else if (flagName == "fixed")
    {
        isFixed = true;
    }
    else
    {
        cout << "Invalid Flag Name\n";
    }
}

bool FlagClass::getFixationFlag()
{
    return isFixed;
}

////////////////////////////////////////////////////////////////////////////////
/*                                  DXL                                       */
////////////////////////////////////////////////////////////////////////////////

DXL::DXL()
{
    port = dynamixel::PortHandler::getPortHandler("/dev/ttyUSB0");
    pkt = dynamixel::PacketHandler::getPacketHandler(2.0);
}

DXL::~DXL()
{
    
}

void DXL::initialize()
{
    // Open port
    if (port->openPort())
    {
        printf("[DXL] ------ Open Port");
    }
    else
    {
        printf("[DXL] Failed to open the port!\n");
        useDXL = false;
        return;
    }

    // Set port baudrate
    if (port->setBaudRate(4500000))
    {
        printf(" ------ change the baudrate!\n");
    }
    else
    {
        printf("\n[DXL] Failed to change the baudrate!\n");
        useDXL = false;
        return;
    }

    for (int id = 0; id < 3; id++) // Dynamixel ID 범위: 0~252
    {
        uint16_t dxl_model_number = 0;
        uint8_t dxl_error = 0;

        int dxl_comm_result = pkt->ping(port, id, &dxl_model_number, &dxl_error);

        if (dxl_comm_result == COMM_SUCCESS && dxl_error == 0)
        {
            printf("[ID:%03d] Found! Model number: %d\n", id, dxl_model_number);
            motorIDs.push_back(static_cast<uint8_t>(id));
            useDXL = true;
        }
    }

    // DXL 토크 ON
    uint8_t err = 0;
    for (uint8_t id : motorIDs)
    {
        pkt->write1ByteTxRx(port, id, 64, 1, &err);
    }
}

void DXL::DXLTorqueOff()
{
    // DXL 토크 Off
    uint8_t err = 0;
    for (uint8_t id : motorIDs)
    {
        pkt->write1ByteTxRx(port, id, 64, 0, &err);
    }
}

void DXL::syncWrite(vector<vector<float>> command)
{
    if (useDXL)
    {
        sw = make_unique<dynamixel::GroupSyncWrite>(port, pkt, 108, 12);

        int numDxl = motorIDs.size();
        for (int i = 0; i < numDxl; i++)
        {
            uint8_t id = motorIDs[i];

            // 모터별 목표 값 배열 정의
            int32_t values_motor[3];
            uint8_t param_motor[12];

            commandToValues(values_motor, command[i]);
            // memcpy를 사용해 정수 배열의 내용을 바이트 배열로 복사
            memcpy(param_motor, values_motor, sizeof(values_motor));
            
            sw->addParam(id, param_motor);
        }

        sw->txPacket();
        sw->clearParam();
    }
}

map<int, float> DXL::syncRead()
{
    map<int, float> dxlData;

    if (useDXL)
    {
        // GroupSyncRead 생성 (주소 132 = Present Position, 길이 4byte)
        dynamixel::GroupSyncRead groupSyncRead(port, pkt, 132, 4);

        // 모터 ID 등록 (예: 1, 2)
        groupSyncRead.addParam(1);
        groupSyncRead.addParam(2);

        // 데이터 요청
        int dxl_comm_result = groupSyncRead.txRxPacket();
        if (dxl_comm_result != COMM_SUCCESS) {
            cerr << "SyncRead failed" << endl;
            return dxlData; // 빈 map 반환
        }

        // 각 모터 값 읽기
        for (int id : {1, 2})
        {
            if (groupSyncRead.isAvailable(id, 132, 4))
            {
                float pos = tickToAngle(groupSyncRead.getData(id, 132, 4));
                dxlData[id] = pos; // ID를 키값으로 저장 (예: dxlData[1] = 2048)
            }
            else
            {
                cerr << "[ID:" << id << "] data not available!" << endl;
            }
        }

        // 다음 사용 위해 clear
        groupSyncRead.clearParam();
    }

    return dxlData;
}

int32_t DXL::angleToTick(float angle)
{
    float degree = angle * 180.0 / M_PI;
    degree = clamp(degree, -180.f, 180.f);
    const float ticks_per_degree = 4096.0 / 360.0;
    float ticks = 2048.0 - (degree * ticks_per_degree);

    return static_cast<int32_t>(round(ticks));
}

float DXL::tickToAngle(int32_t ticks)
{
    const float degrees_per_tick = 360.0 / 4096.0;
    float angle = (2048.0 - static_cast<double>(ticks)) * degrees_per_tick;
    return angle * M_PI / 180.0;
}

void DXL::commandToValues(int32_t values[], vector<float> command)
{
    // Profile Acceleration
    values[0] = static_cast<int32_t>(1000*command[0]);  // ms
    
    // Profile Velocity
    values[1] = static_cast<int32_t>(1000*command[1]);  // ms

    // Goal Position
    values[2] = angleToTick(command[2]);
}

////////////////////////////////////////////////////////////////////////////////
/*                                  Arduino                                   */
////////////////////////////////////////////////////////////////////////////////

Arduino::Arduino()
{

}

Arduino::~Arduino()
{

}

// 연결 함수
bool Arduino::connect(const char* port_name) 
{
    // 이미 연결되어 있다면 아무것도 하지 않음
    if (is_connected) {
        cout << "이미 연결되어 있습니다." << endl;
        return true;
    }

    // 1. 시리얼 포트 열기
    arduino_port = open(port_name, O_RDWR);

    if (arduino_port < 0) {
        cerr << "에러: 시리얼 포트를 열 수 없습니다. (" << port_name << ")" << endl;
        cerr << strerror(errno) << endl;
        return false; // 실패
    }
    
    // 2. 시리얼 포트 설정 (termios 구조체 사용)
    struct termios tty;

    // 현재 포트 설정을 읽어옴
    if (tcgetattr(arduino_port, &tty) != 0) {
        cerr << "에러: 포트 설정을 읽어오는 데 실패했습니다: " << strerror(errno) << endl;
        close(arduino_port); // 포트를 열었으므로 닫아줘야 함
        return false;
    }

    // --- 통신 설정 시작 ---
    // (a) 통신 속도(Baud Rate) 설정: 9600 bps
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // (b) 표준 설정 (8N1: 8 데이터 비트, 패리티 없음, 1 스톱 비트)
    tty.c_cflag &= ~PARENB; // 패리티 비트 비활성화 (No Parity)
    tty.c_cflag &= ~CSTOPB; // 스톱 비트 1개로 설정 (1 Stop bit)
    tty.c_cflag &= ~CSIZE;  // 데이터 비트 크기 필드를 먼저 초기화
    tty.c_cflag |= CS8;     // 데이터 비트 8개로 설정 (8 Data bits)

    // (c) 하드웨어 흐름 제어(Flow Control) 비활성화
    tty.c_cflag &= ~CRTSCTS;

    // (d) 로컬 모드 및 수신 활성화 (필수)
    tty.c_cflag |= CREAD | CLOCAL;

    // (e) 로우(Raw) 모드 설정 (가장 중요)
    // 아두이노와 통신할 때는 운영체제가 데이터를 가공하지 않도록 설정해야 함
    tty.c_lflag &= ~ICANON; // Canonical 모드 비활성화
    tty.c_lflag &= ~ECHO;   // 입력된 문자를 다시 보내지 않음 (Echo off)
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;   // 제어 문자(Ctrl+C 등) 무시

    // (f) 소프트웨어 흐름 제어 비활성화
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // (g) 출력 데이터 가공 비활성화
    tty.c_oflag &= ~OPOST;

    if (tcsetattr(arduino_port, TCSANOW, &tty) != 0) {
        cerr << "에러: 포트 설정을 적용하는 데 실패했습니다." << endl;
        close(arduino_port);
        return false; // 실패
    }

    cout << "시리얼 포트(" << port_name << ") 연결 및 설정 완료." << endl;
    is_connected = true;
    return true; // 성공
}

// 연결 해제 함수
void Arduino::disconnect()
{
    if (is_connected) {
        close(arduino_port);
        is_connected = false;
        arduino_port = -1;
        cout << "시리얼 포트 연결을 해제했습니다." << endl;
    }
}

// 명령 전송 함수
bool Arduino::sendCommand(int command_num)
{
    // 연결이 안 되어있으면 에러
    if (!is_connected) {
        cerr << "에러: 아두이노가 연결되지 않아 명령을 보낼 수 없습니다." << endl;
        return false;
    }

    // int를 char로 변환 (0~9 사이의 숫자만 가능)
    // 예: 숫자 1 -> 문자 '1' (ASCII 49)
    if (command_num < 0 || command_num > 9) {
        cerr << "에러: 0-9 사이의 숫자만 보낼 수 있습니다." << endl;
        return false;
    }
    char msg_to_send = command_num + '0';       // 문자 '0'은 숫자 48에 해당됨 

    cout << arduino_port << endl;
    // 변환된 문자를 아두이노로 전송
    int bytes_written = write(arduino_port, &msg_to_send, 1);

    if (bytes_written < 0) {
        cerr << "에러: 데이터 쓰기에 실패했습니다." << endl;
        return false;
    }

    // cout << "아두이노로 명령 '" << msg_to_send << "' 전송 완료." << endl;
    return true;
}

void Arduino::setHeadLED(Action action)
{
    if (action == POWER_ON && headLED != POWER_ON)
    {
        sendCommand(1);
        headLED = POWER_ON;
    }
    else if (action == IDLE && headLED != IDLE)
    {
        sendCommand(2);
        headLED = IDLE;
    }
    else if (action == PLAYING && headLED != PLAYING)
    {
        sendCommand(3);
        headLED = PLAYING;
    }
    else if (action == POWER_OFF && headLED != POWER_OFF)
    {
        sendCommand(4);
        headLED = POWER_OFF;
    }
}
