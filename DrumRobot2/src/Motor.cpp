// Motor.cpp
#include "../include/motors/Motor.hpp" // Include header file

// For Qt
// #include "../motors/Motor.hpp"
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GenericMotor
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GenericMotor::GenericMotor(uint32_t nodeId)
    :nodeId(nodeId)
    {}


void GenericMotor::clearSendBuffer()
{
    while (!sendBuffer.empty())
    {
        sendBuffer.pop();
    }
}

void GenericMotor::clearReceiveBuffer()
{
    while (!recieveBuffer.empty())
    {
        recieveBuffer.pop();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TMotor
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TMotor::TMotor(uint32_t nodeId, const std::string &motorType)
    : GenericMotor(nodeId), motorType(motorType)
{
}

void TMotor::clearCommandBuffer(){
    std::lock_guard<std::mutex> lock(bufferMutex);
    std::queue<TMotorData>().swap(commandBuffer);
}

float TMotor::jointAngleToMotorPosition(float jointAngle)
{
    float motorPosition;

    if (useFourBarLinkage)
    {
        float L1 = 0.15, L2 = 0.03, L3 = 0.15, L4 = 0.015;

        float alpha = M_PI - jointAngle;
        float L = sqrt(L1*L1 + L4*L4 - 2*L1*L4*cos(alpha));

        float beta = acos((L1*L1 + L*L - L4*L4)/(2*L1*L));
        float gamma = acos((L2*L2 + L*L - L3*L3)/(2*L2*L));

        motorPosition = (beta + gamma - initialMotorAngle) * cwDir;
    }
    else
    {
        motorPosition = (jointAngle - initialJointAngle) * cwDir;
    }

    return motorPosition;
}

float TMotor::motorPositionToJointAngle(float motorPosition)
{
    float jointAngle;

    if (useFourBarLinkage)
    {
        float L1 = 0.15, L2 = 0.03, L3 = 0.15, L4 = 0.015;

        float alpha = motorPosition * cwDir + initialMotorAngle;
        float L = sqrt(L1*L1 + L2*L2 - 2*L1*L2*cos(alpha));

        float beta = acos((L1*L1 + L*L - L2*L2)/(2*L1*L));
        float gamma = acos((L4*L4 + L*L - L3*L3)/(2*L4*L));

        jointAngle = M_PI - (beta + gamma);
    }
    else
    {
        jointAngle = motorPosition * cwDir + initialJointAngle;
    }

    return jointAngle;
}

void TMotor::setInitialMotorAngle(float jointAngle)
{
    if (useFourBarLinkage)
    {
        float L1 = 0.15, L2 = 0.03, L3 = 0.15, L4 = 0.015;

        float alpha = M_PI - jointAngle;
        float L = sqrt(L1*L1 + L4*L4 - 2*L1*L4*cos(alpha));

        float beta = acos((L1*L1 + L*L - L4*L4)/(2*L1*L));
        float gamma = acos((L2*L2 + L*L - L3*L3)/(2*L2*L));

        initialMotorAngle = beta + gamma;
    }
    else
    {
        initialMotorAngle = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// maxonMotor
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MaxonMotor::MaxonMotor(uint32_t nodeId)
    : GenericMotor(nodeId)
{
    // canId 값 설정
    canSendId = 0x600 + nodeId;
    canReceiveId = 0x580 + nodeId;
}

void MaxonMotor::clearCommandBuffer(){
    std::lock_guard<std::mutex> lock(bufferMutex);
    std::queue<MaxonData>().swap(commandBuffer);
}

float MaxonMotor::jointAngleToMotorPosition(float jointAngle)
{
    float motorPosition;
    
    motorPosition = (jointAngle - initialJointAngle) * cwDir;

    return motorPosition;
}

float MaxonMotor::motorPositionToJointAngle(float motorPosition)
{
    float jointAngle;
    
    jointAngle = motorPosition * cwDir + initialJointAngle;

    return jointAngle;
}