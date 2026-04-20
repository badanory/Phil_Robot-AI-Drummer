//AgentAction.cpp
#include "../include/tasks/AgentAction.hpp"

// 기존 매니저들 참조를 위해 포함
#include "../include/managers/PathManager.hpp"
#include "../include/tasks/DrumRobot.hpp" // FlagClass, Arduino 등 타입 참조용
#include "../include/motors/Motor.hpp" // 모터 제어를 위해 필요

// 생성자: 로봇 제어 모듈들의 참조를 저장
AgentAction::AgentAction(PathManager& pathRef,
                        FlagClass& flagRef,
                        Arduino& arduinoRef,
                        std::map<std::string, std::shared_ptr<GenericMotor>>& motorsRef)
    : pathManager(pathRef),
    flagObj(flagRef),
    arduino(arduinoRef),
    motors(motorsRef)
{
    std::cout << "[AgentAction] Policy Module Initialized." << std::endl;
}

AgentAction::~AgentAction() {}

// =============================================================
// [핵심] 명령어 해석기 (Command Dispatcher)
// =============================================================
void AgentAction::executeCommand(std::string fullCmd)
{
    // 1. 전처리: "[CMD:look:10,10]" -> "look:10,10"
    std::string cleanCmd = cleanCommand(fullCmd);
    
    // 2. 분리: "look:10,10" -> ["look", "10,10"]
    std::vector<std::string> tokens = split(cleanCmd, ':');
    if (tokens.empty()) return;

    std::string action = tokens[0]; // 메인 행동 키워드

    // 3. 정책 매핑 (Routing)
    // (1) ex)시선 제어: look:pan,tilt(수평, 수직)-> home: 0,90 (정면)
    if (action == "look" && tokens.size() >= 2) 
    {
        std::vector<std::string> angles = split(tokens[1], ',');
        if (angles.size() == 2) {
            try {
                float pan = std::stof(angles[0]);
                float tilt = std::stof(angles[1]);
                policy_lookAt(pan, tilt);
            } catch (...) { std::cerr << "[Agent] Look parsing error\n"; }
        }
    }
    // (2) ex)제스처: gesture:nod
    else if (action == "gesture" && tokens.size() >= 2)
    {
        policy_gesture(tokens[1]);
    }
    /*
    // (3) ex)자세변경: pose:ready
    else if (action == "pose" && tokens.size() >= 2)
    {
        policy_pose(tokens[1]);
    }
    */
    // (4) ex)개별 관절 이동: move:R_arm1,45
    else if (action == "move" && tokens.size() >= 2)
    {
        // tokens[1]은 "R_arm1,45" 형태
        std::vector<std::string> params = split(tokens[1], ',');
        if (params.size() == 2)
        {
            std::string motorName = params[0];
            try {
                float angle = std::stof(params[1]);
                policy_moveJoint(motorName, angle);
            } catch (...) { std::cerr << "[Agent] Move parsing error\n"; }
        }
    }
    else 
    {
        std::cout << "[Agent] Unknown Action: " << action << std::endl;
    }
}

std::vector<std::string> AgentAction::unpackCommands(const std::string& payload)
{
    std::vector<std::string> commands;
    std::string trimmedPayload = trim(payload);
    if (trimmedPayload.empty()) {
        return commands;
    }

    if (!trimmedPayload.empty() && trimmedPayload.front() == '{') {
        if (!parseJsonCommandPayload(trimmedPayload, commands)) {
            std::cerr << "[Agent] JSON payload parsing failed: " << trimmedPayload << std::endl;
        }
        return commands;
    }

    commands.push_back(trimmedPayload);
    return commands;
}

// =============================================================
// Action Policies (행동 구체화)
// =============================================================

void AgentAction::policy_lookAt(float pan, float tilt)
{
    std::cout << "[Action] Look At -> Pan:" << pan << ", Tilt:" << tilt << std::endl;

    float targetPan  = pan  * M_PI / 180.0f;
    float targetTilt = tilt * M_PI / 180.0f;
    float move_time  = 1.0f; // 1초 이동
    float dt         = 0.005f; // 5ms 한 스텝

    // PathManager genDxlTrajectory와 동일하게 5ms 단위 보간 trajectory를 push한다.
    // SIL에서도 중간 각도가 tick마다 전달되어 부드러운 움직임으로 보인다.
    for (float t = dt; t <= move_time + 1e-6f; t += dt)
    {
        float tau   = t / move_time;
        float interp = (1.0f - cosf(M_PI * tau)) / 2.0f; // 사인 보간

        float curPan  = lastPanRad  + (targetPan  - lastPanRad)  * interp;
        float curTilt = lastTiltRad + (targetTilt - lastTiltRad) * interp;

        std::vector<std::vector<float>> dxlCmd;
        dxlCmd.push_back({dt, dt, curPan});
        dxlCmd.push_back({dt, dt, curTilt});
        
        {
            std::lock_guard<std::mutex> lock(pathManager.dxlBufferMutex);
            pathManager.dxlCommandBuffer.push(dxlCmd);
        }
    }

    lastPanRad  = targetPan;
    lastTiltRad = targetTilt;
}

void AgentAction::policy_gesture(std::string type)
{
    std::cout << "[Action] Perform Gesture: " << type << std::endl;

    // 1. 인사 (Wave) - 팔을 올려 인사 자세를 잡은 뒤 손목만 빠르게 흔든다.
    if (type == "wave" || type == "hi")
    {
        // 인사 방향으로 시선
        policy_lookAt(20, 95);
        // 팔을 인사 자세로 올리기 (정상 속도, 각 관절 동시 이동)
        policy_moveJoint("R_arm1", 45.0);
        policy_moveJoint("R_arm2", 45.0);   // 기존 20 -> 45: 팔을 더 높이 올려 인사처럼 보이게
        policy_moveJoint("R_arm3", 90.0);
        policy_moveJoint("R_wrist", 0.0);
        // 끄덕임: 아래 -> 위 -> 정면
        // 손목만 2배 빠르게 (1.0초) 4회 왕복 흔들기
        policy_moveJoint("R_arm3", 110.0, 1.0f);
        policy_moveJoint("R_wrist",  25.0, 1.0f);
        policy_moveJoint("R_arm3",  70.0, 1.0f);
        policy_moveJoint("R_wrist", -25.0, 1.0f);

        policy_moveJoint("R_arm3", 110.0, 1.0f);
        policy_moveJoint("R_wrist",  25.0, 1.0f);

        policy_moveJoint("R_arm3",  70.0, 1.0f);
        policy_moveJoint("R_wrist", -25.0, 1.0f);
        policy_moveJoint("R_arm3", 110.0, 1.0f);
        policy_moveJoint("R_wrist",  25.0, 1.0f);
        policy_moveJoint("R_arm3",  90.0, 1.0f);
        policy_moveJoint("R_wrist",   0.0, 1.0f);

        //policy_moveJoint("R_wrist", -25.0, 1.0f);
        //policy_moveJoint("R_wrist",  25.0, 1.0f);
    }
    // 2. 끄덕임 (Nod) - DXL 모터 사용
    else if (type == "nod"){
        // 끄덕임: 아래 -> 위 -> 정면
        policy_lookAt(0, 70); // 아래
        // 연속 동작은 큐에 순차적으로 넣으면 됨 (PathManager 구조상)
        // 실제로는 시간 지연이 필요할 수 있음.
        policy_lookAt(0, 110); // 위 
        // V1에서는 단순하게 구현.
        policy_lookAt(0, 90);  // 정면
    }
    // 3. 도리도리 (Shake) - 좌우로 고개 흔들기
    else if (type == "shake"){
        policy_lookAt(-30, 90);
        policy_lookAt(30, 90);
        policy_lookAt(0, 90);
    }
    // 4. 환호 (Hurray) - 양팔을 조금 벌리고 팔꿈치를 세워 만세 느낌을 만든다.
    else if (type == "hurray" || type == "happy") {
        policy_moveJoint("R_arm1", 60.0);
        policy_moveJoint("L_arm1", 120.0);
        policy_moveJoint("R_arm2", 65.0);
        policy_moveJoint("L_arm2", 65.0);
        policy_moveJoint("R_arm3", 95.0);
        policy_moveJoint("L_arm3", 95.0);
        policy_moveJoint("R_wrist", 0.0);
        policy_moveJoint("L_wrist", 0.0);
        policy_lookAt(0, 105);
    }
}

/*
void AgentAction::policy_pose(std::string pose)
{
    std::cout << "[Action] Set Pose: " << pose << std::endl;

    if (pose == "ready") {
        flagObj.setAddStanceFlag(FlagClass::READY);
        // 상태 변경 트리거는 DrumRobot 메인 루프에서 처리되므로 플래그만 세팅
    }
    else if (pose == "home") {
        flagObj.setAddStanceFlag(FlagClass::HOME);
    }
    else if (pose == "shutdown") {
        flagObj.setAddStanceFlag(FlagClass::SHUTDOWN);
    }
}
*/

void AgentAction::policy_moveJoint(std::string motorName, float angleDeg, float moveTime)
{
    // 1. 모터 이름 확인
    if (motors.find(motorName) == motors.end()) {
        std::cerr << "[Agent] Unknown Motor: " << motorName << std::endl;
        return;
    }
    std::cout << "[Action] Move " << motorName << " to " << angleDeg << " deg" << std::endl;

    auto motor = motors[motorName];
    float targetRad = angleDeg * M_PI / 180.0f;
    float move_time = moveTime;
    float dt = 0.005f;      // 5ms 주기 (CAN 통신 주기)

    // 2. 모터 타입별 명령 생성 (GenericMotor -> TMotor/MaxonMotor 캐스팅)
    // T-Motor 처리
    if (auto tMotor = std::dynamic_pointer_cast<TMotor>(motor)) 
    {
        // [안전장치] 각도 제한 확인
        if (targetRad < tMotor->rMin || targetRad > tMotor->rMax) {
            std::cerr << "[Warning] Joint Limit Exceeded (T-Motor): " << motorName << " Curr: " << angleDeg << " Limit: " << tMotor->rMin << "~" << tMotor->rMax << std::endl;
            return;
        }

        // [보간 준비] 시작점과 목표점 계산 (Radian 단위)
        float startRad = tMotor->motorPositionToJointAngle(tMotor->finalMotorPosition); // 현재 위치를 기준으로 보간 시작
        
        std::lock_guard<std::mutex> lock(tMotor->bufferMutex); // T-Motor 전용 자물쇠 

        for (float t = dt; t <= move_time; t += dt) {
            
            // 사인 보간 공식: current = (target - start) / 2 * cos(pi * t / T + pi) + (target + start) / 2
            float current_rad = ((targetRad - startRad) / 2.0f) * cos(M_PI * (t / move_time + 1.0f)) + ((targetRad + startRad) / 2.0f);

            // T-Motor 명령 생성
            TMotorData newData;
            newData.position = tMotor->jointAngleToMotorPosition(current_rad);
            newData.mode = tMotor->Position;
            newData.velocityERPM = 0;
            newData.useBrake = 0;

            // T-Motor 명령 버퍼에 추가
            tMotor->commandBuffer.push(newData);
            tMotor->finalMotorPosition = newData.position; // 최종 위치 업데이트 (다음 명령의 시작점이 됨)
        }
    }

    // Maxon 모터 처리
    else if (auto maxonMotor = std::dynamic_pointer_cast<MaxonMotor>(motor))
    {
        // [안전장치] 각도 제한 확인
        if (targetRad < maxonMotor->rMin || targetRad > maxonMotor->rMax) {
            std::cerr << "[Warning] Joint Limit Exceeded (Maxon): " << motorName << " Curr: " << angleDeg << " Limit: " << maxonMotor->rMin << "~" << maxonMotor->rMax << std::endl;
            return;
        }

        // [보간 준비] 시작점과 목표점 계산
        // finalMotorPosition은 이전에 마지막으로 명령 내린 위치입니다.
        float startRad = maxonMotor->motorPositionToJointAngle(maxonMotor->finalMotorPosition); // 현재 위치를 기준으로 보간 시작

        std::lock_guard<std::mutex> lock(maxonMotor->bufferMutex);

        for (float t = dt; t <= move_time; t += dt) {
            
            // 사인 보간 공식: start + (target - start) * (1 - cos(pi * t / T)) / 2
            float current_rad = startRad + (targetRad - startRad) * (1 - cos(M_PI * t / move_time)) / 2.0f;

            // Maxon 모터 전용 1ms 5단계 쪼개기
            for (int i = 0; i < 5; i++) { 
                
                MaxonData newData;
                newData.position = maxonMotor->jointAngleToMotorPosition(current_rad);
                newData.mode = maxonMotor->CSP;
                newData.kp = 0;
                newData.kd = 0;

                maxonMotor->commandBuffer.push(newData);
                maxonMotor->finalMotorPosition = newData.position; // 최종 위치 업데이트 (다음 명령의 시작점이 됨)
            }
        }
    }
    else 
    {
        std::cerr << "[Agent] Unsupported Motor Type for: " << motorName << std::endl;
    }

}

// =============================================================
// Utilities
// =============================================================

std::vector<std::string> AgentAction::split(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string AgentAction::cleanCommand(std::string cmd)
{
    // "[CMD:look...]" -> "look..."
    size_t start = cmd.find("CMD:");
    if (start != std::string::npos) {
        cmd = cmd.substr(start + 4);
    }
    // 닫는 대괄호 제거
    size_t end = cmd.find("]");
    if (end != std::string::npos) {
        cmd = cmd.substr(0, end);
    }
    return cmd;
}

std::string AgentAction::trim(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }

    return value.substr(start, end - start);
}

bool AgentAction::parseJsonCommandPayload(const std::string& payload, std::vector<std::string>& commands)
{
    std::vector<std::string> parsedCommands;
    if (extractJsonStringArray(payload, "commands", parsedCommands)) {
        for (const auto& cmd : parsedCommands) {
            std::string trimmedCmd = trim(cmd);
            if (!trimmedCmd.empty()) {
                commands.push_back(trimmedCmd);
            }
        }
        return true;
    }

    std::string singleCommand;
    if (extractJsonStringValue(payload, "command", singleCommand)) {
        singleCommand = trim(singleCommand);
        if (!singleCommand.empty()) {
            commands.push_back(singleCommand);
        }
        return true;
    }

    return false;
}

bool AgentAction::extractJsonStringArray(const std::string& payload, const std::string& key, std::vector<std::string>& values)
{
    std::string token = "\"" + key + "\"";
    size_t keyPos = payload.find(token);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = payload.find(':', keyPos + token.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t pos = colonPos + 1;
    while (pos < payload.size() && std::isspace(static_cast<unsigned char>(payload[pos]))) {
        pos++;
    }

    if (pos >= payload.size() || payload[pos] != '[') {
        return false;
    }
    pos++;

    while (pos < payload.size()) {
        while (pos < payload.size() && std::isspace(static_cast<unsigned char>(payload[pos]))) {
            pos++;
        }

        if (pos >= payload.size()) {
            return false;
        }
        if (payload[pos] == ']') {
            return true;
        }
        if (payload[pos] == ',') {
            pos++;
            continue;
        }
        if (payload[pos] != '"') {
            return false;
        }

        std::string parsedValue;
        size_t nextPos = pos;
        if (!readJsonString(payload, pos, parsedValue, nextPos)) {
            return false;
        }

        values.push_back(parsedValue);
        pos = nextPos;

        while (pos < payload.size() && std::isspace(static_cast<unsigned char>(payload[pos]))) {
            pos++;
        }

        if (pos < payload.size() && payload[pos] == ',') {
            pos++;
        }
    }

    return false;
}

bool AgentAction::extractJsonStringValue(const std::string& payload, const std::string& key, std::string& value)
{
    std::string token = "\"" + key + "\"";
    size_t keyPos = payload.find(token);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = payload.find(':', keyPos + token.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t pos = colonPos + 1;
    while (pos < payload.size() && std::isspace(static_cast<unsigned char>(payload[pos]))) {
        pos++;
    }

    if (pos >= payload.size() || payload[pos] != '"') {
        return false;
    }

    size_t nextPos = pos;
    return readJsonString(payload, pos, value, nextPos);
}

bool AgentAction::readJsonString(const std::string& payload, size_t startQuote, std::string& value, size_t& nextPos)
{
    if (startQuote >= payload.size() || payload[startQuote] != '"') {
        return false;
    }

    value.clear();
    bool escape = false;

    for (size_t i = startQuote + 1; i < payload.size(); ++i) {
        char current = payload[i];

        if (escape) {
            switch (current) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(current); break;
            }
            escape = false;
            continue;
        }

        if (current == '\\') {
            escape = true;
            continue;
        }

        if (current == '"') {
            nextPos = i + 1;
            return true;
        }

        value.push_back(current);
    }

    return false;
}
