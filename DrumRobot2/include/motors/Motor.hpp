#ifndef MOTOR_H
#define MOTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <stdlib.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <linux/can/raw.h>
#include <iostream>
#include <cmath>
#include <map>
#include <mutex> // For std::mutex()
using namespace std;

class GenericMotor
{
public:
    GenericMotor(uint32_t nodeId);
    virtual ~GenericMotor() = default;

    // For CAN communication
    uint32_t nodeId;
    int socket = -1;
    bool isConected = false;

    // Motors Feature
    float cwDir;
    float rMin, rMax;
    std::string myName;

    // Receive (stateMutex로 보호: recvThread 쓰기 / sendThread 읽기)
    float motorPosition, motorVelocity;
    float jointAngle;
    float initialJointAngle;
    std::mutex stateMutex;

    // parseSendCommand() -> 현재 사용 안함
    float desPos, desVel, desTor;

    // 명령의 최종 위치 저장
    float finalMotorPosition;

    std::queue<can_frame> sendBuffer;
    std::queue<can_frame> recieveBuffer;

    struct can_frame sendFrame;
    struct can_frame recieveFrame;

    void clearSendBuffer();
    void clearReceiveBuffer();

    ///////////////////////////////////////////////////////////

    // 에러 (쓰이는 곳은 없음)
    bool isError = false;

    //판별함수
    virtual bool isTMotor() const { return false; }  // 기본값: false
    virtual bool isMaxonMotor() const { return false; }  // 기본값: false
};

struct TMotorData
{
    float position;
    float velocityERPM = 0.0;
    int mode;
    int useBrake;
    bool is_measure_end = false;  // [★ 추가] 한 마디의 끝 표시
    bool is_last_measure = false; // [★ 추가] 전체 연주의 진짜 마지막 표시
};

class TMotor : public GenericMotor
{
public:
    TMotor(uint32_t nodeId, const std::string &motorType);
    
    std::string motorType;

    // Receive
    float motorCurrent;

    // Current Limit
    float currentLimit;
    int currentErrorCnt = 0;

    // 제어 모드
    const int Position = 0;
    const int Velocity = 2;
    // const int VelocityFB = 3;    // 사용 안함
    // const int VelocityFF = 4;    // 사용 안함

    int controlMode = Position;

    // 극수
    const float pole = 21.0;

    // 기어비
    float gearRatio;

    // 속도 제어 시 위치 오차 게인
    float positionGain = 1000.0;  

    // commandBuffer
    std::queue<TMotorData> commandBuffer;
    std::mutex bufferMutex; // [★ 추가] T-Motor 전용 자물쇠
    void clearCommandBuffer();

    bool useFourBarLinkage;
    float initialMotorAngle;    // Four Bar Linkage 사용시 모터의 초기 위치
    float jointAngleToMotorPosition(float jointAngle);
    float motorPositionToJointAngle(float motorPosition);
    void setInitialMotorAngle(float jointAngle);

    bool isTMotor() const override { return true; }

private:
};

struct MaxonData
{
    float position;
    int mode;
    int kp;
    int kd;
    bool is_measure_end = false;  // [★ 추가] 한 마디의 끝 표시
    bool is_last_measure = false; // [★ 추가] 자연스러운 종료를 위한 마지막 데이터 꼬리표
};

class MaxonMotor : public GenericMotor
{
public:
    MaxonMotor(uint32_t nodeId);

    uint32_t canSendId;
    uint32_t canReceiveId;

    uint32_t txPdoIds[4];
    uint32_t rxPdoIds[4];

    // Receive
    float motorTorque;
    unsigned char statusBit;

    // 제어 모드
    const int HMM = 0;  // Homming Mode
    const int CSP = 1;  // Cyclic Sync Position Mode
    const int CSV = 2;  // Cyclic Sync Velocity Mode
    const int CST = 3;  // Cyclic Sync Torque Mode

    int controlMode = CSP;    // 현재 제어 모드

    // 1ms 궤적 만들기 위해 사용
    float pre_q;

    // 마찰 토크 보상 시 사용되는 값
    float preMotorPosition;

    // 타격 감지용 변수
    // queue<double> positionValues; // 포지션 값 저장을 위한 queue
    // int maxIndex = 5;
    bool hitting = false; // 드럼이 실제로 타격일 때 켜짐
    float hittingPos = 0; // 타격 시점의 각도
    // float hittingDrumAngle = 0; // 드럼 별 타격 각도
    bool isHit = false; // 타격 궤적 시작 (내려가는 궤적)
    bool drumReached = false; // 올라오는 궤적 시작

    // 입력 토크 계산시 사용되는 에러
    float pre_err = 0;

    // commandBuffer
    queue<MaxonData> commandBuffer;
    std::mutex bufferMutex; // [★ 추가] Maxon 전용 자물쇠
    void clearCommandBuffer();

    float jointAngleToMotorPosition(float jointAngle);
    float motorPositionToJointAngle(float motorPosition);

    bool isMaxonMotor() const override { return true; }
};

#endif // MOTOR_H
