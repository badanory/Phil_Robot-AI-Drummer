#pragma once

#include <stdio.h>
#include "../include/managers/CanManager.hpp"
#include "../include/motors/CommandParser.hpp"
#include "../include/motors/Motor.hpp"
#include "../include/tasks/SystemState.hpp"
#include "../include/USBIO_advantech/USBIO_advantech.hpp"
#include "../include/tasks/Functions.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <functional>
#include <queue>
#include <algorithm>
#include <thread>
#include <cerrno>  // errno
#include <cstring> // strerror
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <iostream>
#include <vector>
#include <limits>
#include <ctime>
#include <fstream>
#include <atomic>
#include <cmath>
#include <chrono>
#include <set>
#include <numeric>
#include <signal.h>
#include "../include/eigen-3.4.0/Eigen/Dense"

//For Qt
/*
#include "CanManager.hpp"
#include "../motors/CommandParser.hpp"
#include "../motors/Motor.hpp"
#include "../tasks/SystemState.hpp"
*/

using namespace std;
using namespace Eigen;

/**
 * @class PathManager
 * @brief 드럼 로봇의 연주 경로를 생성하는 부분을 담당하는 클래스입니다.
 *
 * 이 클래스는 주어진 악보를 분석하여 정해진 알고리즘을 따라 알맞은 경로를 생성하도록 합니다.
 */

class PathManager
{

public:
    PathManager(State &stateRef,
                CanManager &canManagerRef,
                std::map<std::string, std::shared_ptr<GenericMotor>> &motorsRef,
                USBIO &usbioRef,
                Functions &funcRef);
    
    bool endOfPlayCommand = false;
    bool startOfPlay = false;
    double bpmOfScore = 100.0;      ///< 악보의 BPM 정보.
    string maxonMode = "unknown";
    int Kp, Kd;
    double kpMin;
    double kpMax;
    int KdDrop;
    string tmotorMode = "velocity";

    void initPathManager();
    void genAndPushAddStance(string flagName);
    void initPlayStateValue();
    void processLine(MatrixXd &measureMatrix);
    void clearCommandBuffers();

    // DXL
    std::queue<vector<vector<float>>> dxlCommandBuffer;
    std::mutex dxlBufferMutex; // [★ 추가] DXL 모터 전용 자물쇠

    /////////////////////////////////////////////////////////////////////////// Init
    void setDrumCoordinate();   // 임시로 이동 private -> public
    void setAddStanceAngle();

private:
    TMotorCommandParser TParser; ///< T 모터 명령어 파서.
    MaxonCommandParser MParser;  ///< Maxon 모터 명령어 파서

    State &state;                                                 ///< 시스템의 현재 상태입니다.
    CanManager &canManager;                                       ///< CAN 통신을 통한 모터 제어를 담당합니다.
    std::map<std::string, std::shared_ptr<GenericMotor>> &motors; ///< 연결된 모터들의 정보입니다.
    USBIO &usbio;
    Functions &func;

    /////////////////////////////////////////////////////////////////////////// struct def
    // 로봇 링크 길이
    typedef struct{

        // float upperArm = 0.250;         ///< 상완 길이. [m]
        float upperArm = 0.230;         ///< 상완 길이. [m]
        // float lowerArm = 0.328;         ///< 하완 길이. [m]
        // float lowerArm = 0.178;         ///< 하완 길이. [m]
        float lowerArm = 0.200;         ///< 하완 길이. [m]
        float stick = 0.325+0.048;      ///< 스틱 길이 + 브라켓 길이. [m]
        float waist = 0.520;            ///< 허리 길이. [m]
        float height = 1.020-0.0605;    ///< 바닥부터 허리까지의 높이. [m]

        // TestManager.hpp 에서도 수정해줘야 함.

    }PartLength;

    // measureMatrix 파싱한 데이터
    typedef struct {
        double t1, t2;    // 궤적 생성 시간

        double initialTimeR, finalTimeR;       // 전체 궤적에서 출발 시간, 도착 시간
        double initialTimeL, finalTimeL;

        VectorXd initialPositionR, finalPositionR;  // 전체 궤적에서 출발 위치
        VectorXd initialPositionL, finalPositionL;  // 전체 궤적에서 도착 위치

        double initialWristAngleR, finalWristAngleR;    // 손목 각도 출발 위치
        double initialWristAngleL, finalWristAngleL;    // 손목 각도 도착 위치

        VectorXd nextStateR;            // 이전 시간, 이전 악기, 상태
        VectorXd nextStateL;

    }TrajectoryData;

    // task space 궤적
    typedef struct {
        VectorXd trajectoryR; ///< 오른팔 스틱 끝 좌표 (x, y, z)
        VectorXd trajectoryL; ///< 왼팔 스틱 끝 좌표 (x, y, z)

        double wristAngleR;  ///> IK를 풀기 위한 오른 손목 각도
        double wristAngleL;  ///> IK를 풀기 위한 왼 손목 각도
    }TaskSpaceTrajectory;


    //DXL 궤적
    typedef struct {
        float   dxl1;
        float   dxl2;
    }DXLTrajectory;    

    // 허리 파라미터
    typedef struct {
        int n;                  // 명령 개수
        double min_q0;          // 최소
        double max_q0;          // 최대
        double optimized_q0;    // 최적화
    }WaistParameter;

    // 타격 파라미터
    typedef struct {
        
        double initialTimeR = 0, finalTimeR;    // 타격 궤적에서 출발 시간, 도착 시간
        double initialTimeL = 0, finalTimeL;

        VectorXd nextStateR;            // 이전 시간, 이전 State, intensity
        VectorXd nextStateL;
        
    }HitData;

    // 타격 궤적
    typedef struct {
        double elbowR;  ///> 오른팔 팔꿈치 관절에 더해줄 각도
        double elbowL;  ///> 왼팔 팔꿈치 관절에 더해줄 각도
        double wristR;  ///> 오른팔 손목 관절에 더해줄 각도
        double wristL;  ///> 왼팔 손목 관절에 더해줄 각도

        double kpRatioR = 1.0;
        double kpRatioL = 1.0;
    }HitTrajectory;

    // 페달 궤적
    typedef struct {
        double bass;    ///> 오른발 관절에 더해줄 각도
        double hihat;   ///> 왼발 관절에 더해줄 각도
    }PedalTrajectory;

    typedef struct {
        double stayTime;
        double liftTime;
        double hitTime;     // 전체 시간
    }ElbowTime;

    typedef struct {
        double releaseTime;
        double stayTime;
        double liftTime;
        double hitTime;     // 전체 시간
    }WristTime;

    typedef struct {
        double stayTime;
        double liftTime;
        double hitTime;
    }BassTime;
    
    typedef struct {
        double settlingTime;
        double liftTime;
        double hitTime;
        double splashTime;
    }HihatTime;

    typedef struct {
        double stayAngle = 0.0;
        double liftAngle = 15*M_PI/180.0;
    }ElbowAngle;

    typedef struct {
        double stayAngle = 20*M_PI/180.0;
        double pressAngle = -5*M_PI/180.0;
        double liftAngle = 40*M_PI/180.0;
    }WristAngle;

    typedef struct {
        double stayAngle = 0*M_PI/180.0;
        double pressAngle = -20*M_PI/180.0;
    }BassAngle;

    typedef struct {
        double openAngle = -3*M_PI/180.0;
        double closedAngle = -13*M_PI/180.0;
    }HihatAngle;

    /////////////////////////////////////////////////////////////////////////// Init
    MatrixXd drumCoordinateR;               ///< 오른팔의 각 악기별 위치 좌표 벡터.
    MatrixXd drumCoordinateL;               ///< 왼팔의 각 악기별 위치 좌표 벡터.

    MatrixXd wristAngleOnImpactR;           ///< 오른팔의 각 악기별 타격 시 손목 각도.
    MatrixXd wristAngleOnImpactL;           ///< 왼팔의 각 악기별 타격 시 손목 각도.

    // void setDrumCoordinate();
    void setWristAngleOnImpact();
    // void setAddStanceAngle();
    void setReadyAngle();

    /////////////////////////////////////////////////////////////////////////// AddStance
    VectorXd readyAngle;                    ///< AddStace 에서 사용하는 위치 (자세)
    VectorXd homeAngle;
    VectorXd shutdownAngle;

    // 사다리꼴 속도 프로파일 생성을 위한 가속도
    const float accMax = 100.0; // rad/s^2

    VectorXd getFinalMotorPosition();
    VectorXd getAddStanceAngle(string flagName);
    void pushAddStance(VectorXd &Q1, VectorXd &Q2);
    VectorXd calVmax(VectorXd &q1, VectorXd &q2, float t2);  // q1[rad], q2[rad], t2[s]
    VectorXd makeProfile(VectorXd &q1, VectorXd &q2, VectorXd &Vmax, float t, float t2); // q1[rad], q2[rad], Vmax[rad/s], t[s], t2[s]
    VectorXd makeVelocityProfile(VectorXd &q1, VectorXd &q2, VectorXd &Vmax, float t, float t2);
    void pushAddStanceDXL(string flagName);

    /////////////////////////////////////////////////////////////////////////// Play
    int lineOfScore = 0;                    ///< 현재 악보 읽은 줄.
    const int preCreatedLine = 3;           ///< 미리 궤적을 생성할 줄

    void avoidCollision(MatrixXd &measureMatrix);
    void genTrajectory(MatrixXd &measureMatrix);
    void solveIKandPushCommand();

    //////////////////////////////////// Task Space Trajectory
    double roundSum = 0.0;                  ///< 5ms 스텝 단위에서 누적되는 오차 보상
    VectorXd measureStateR, measureStateL;  ///< [이전 시간, 이전 악기, 상태] 0 : 0 <- 0 / 1 : 0 <- 1 / 2 : 1 <- 0 / 3 : 1 <- 1
    queue<TaskSpaceTrajectory> taskSpaceQueue;
    queue<WaistParameter> waistParameterQueue;

    int getNumCommands(MatrixXd &measureMatrix);
    void genTaskSpaceTrajectory(MatrixXd &measureMatrix, int n);
    PathManager::TrajectoryData getTrajectoryData(MatrixXd &measureMatrix, VectorXd &stateR, VectorXd &stateL);
    pair<VectorXd, VectorXd> parseTrajectoryData(VectorXd &t, VectorXd &inst, VectorXd &hihat, VectorXd &stateVector);
    int checkOpenHihat(int instNum, int isHihat);
    pair<VectorXd, double> getTargetPosition(VectorXd &inst, char RL);
    double calTimeScaling(double ti, double tf, double t);
    VectorXd makeTaskSpacePath(VectorXd &Pi, VectorXd &Pf, double s);
    VectorXd getWaistParams(VectorXd &pR, VectorXd &pL, double theta7, double theta8);
    void storeWaistParams(int n, VectorXd &waistParams);

    //////////////////////////////////// Hit Trajectory
    VectorXd prevLine = VectorXd::Zero(9);  ///< 악보 나눌 때 시작 악보 기록
    VectorXd hitStateR, hitStateL;          ///< [이전 시간, 이전 State, intensity]
    ElbowTime elbowTimeR, elbowTimeL;
    WristTime wristTimeR, wristTimeL;
    MatrixXd elbowCoefficientR;
    MatrixXd elbowCoefficientL;
    MatrixXd wristCoefficientR;
    MatrixXd wristCoefficientL;
    queue<HitTrajectory> hitQueue;

    void genHitTrajectory(MatrixXd &measureMatrix, int n);
    MatrixXd divideMatrix(MatrixXd &measureMatrix);
    PathManager::HitData getHitData(MatrixXd &measureMatrix, VectorXd &stateR, VectorXd &stateL);
    VectorXd parseHitData(VectorXd &t, VectorXd &hit, double preT, double preStatem, double hitDetectionThreshold);
    void makeHitCoefficient(HitData hitData, VectorXd &stateR, VectorXd &stateL);
    PathManager::ElbowTime getElbowTimeParam(double t1, double t2, int intensity);
    PathManager::WristTime getWristTimeParam(double t1, double t2, int intensity, int state);
    PathManager::ElbowAngle getElbowAngleParam(double t1, double t2, int intensity);
    PathManager::WristAngle getWristAngleParam(double t1, double t2, int intensity);
    MatrixXd makeElbowCoefficient(int state, ElbowTime eT, ElbowAngle eA);
    MatrixXd makeWristCoefficient(int state, WristTime wT, WristAngle wA);
    double getElbowAngle(double t, ElbowTime eT, MatrixXd &coefficientMatrix);
    double getWristAngle(double t, WristTime wT, MatrixXd &coefficientMatrix);
    double getKpRatio(double t, WristTime wT);

    //////////////////////////////////// Pedal Trajectory
    queue<PedalTrajectory> pedalQueue;

    void genPedalTrajectory(MatrixXd &measureMatrix, int n);
    int getBassState(bool bassHit, bool nextBaseHit);
    PathManager::BassTime getBassTime(float t1, float t2);
    int getHihatState(bool hihatClosed, bool nextHihatClosed);
    PathManager::HihatTime getHihatTime(float t1, float t2);
    double getBassAngle(double t, BassTime bt, int bassState);
    double getHihatAngle(double t, HihatTime ht, int hihatState, int nextHihatClosed);
    double makeCosineProfile(double qi, double qf, double ti, double tf, double t);

    //////////////////////////////////// DXL Trajectory
    queue<DXLTrajectory> DXLQueue;

    void genDxlTrajectory(MatrixXd &measureMatrix, int n);
    float getInstAngle(int nextInst);
    double getNodIntensity(MatrixXd &measureMatrix);
    float makeNod(double beatOfLine, double nodIntensity, int i, int n);

    //////////////////////////////////// Solve IK & Push Command Buffer
    double waistAngleT1;                    ///< 허리 시작 위치
    double waistAngleT0, waistTimeT0 = -1;  ///< 허리 이전 위치, 이전 시간
    float prevWaistPos = 0.0;   ///< 브레이크 판단에 사용될 허리 전 값
    float preDiff = 0.0;        ///< 브레이크 판단(필터)에 사용될 전 허리 차이값

    std::vector<PathManager::WaistParameter> waistParamsQueueToVector();
    MatrixXd makeWaistCoefficient(std::vector<WaistParameter> &waistParams);
    std::pair<double, vector<double>> getWaistAngleT2(std::vector<WaistParameter> &waistParams);
    vector<double> cubicInterpolation(const vector<double>& q, const vector<double>& t);
    double getWaistAngle(MatrixXd &waistCoefficient, int index);
    VectorXd getJointAngles(double q0, double &KpRatioR, double &KpRatioL);
    void pushDxlBuffer(double q0);
    void pushCommandBuffer(VectorXd &Qi, double KpRatioR, double KpRatioL);
    float getVelocityRadps(bool restart, double q, int can_id);

    //////////////////////////////////// Detect Collision
    std::string tablePath = "../include/table/TABLE.bin";    // 테이블 위치

    bool detectCollision(MatrixXd &measureMatrix);
    int findDetectionRange(MatrixXd &measureMatrix);
    bool checkTable(VectorXd PR, VectorXd PL, double hitR, double hitL);
    size_t getFlattenIndex(const std::vector<size_t>& indices, const std::vector<size_t>& dims);
    std::pair<size_t, size_t> getBitIndex(size_t offsetIndex);

    //////////////////////////////////// Avoid Collision
    map<int, std::string> modificationMethods = { ///< 악보 수정 방법 중 우선 순위
        { 0, "Crash"},
        { 1, "WaitAndMove"},
        { 2, "MoveAndWait"},
        { 3, "Switch"},
        { 4, "Delete"}
    };

    bool modifyMeasure(MatrixXd &measureMatrix, int priority);
    pair<int, int> findModificationRange(VectorXd t, VectorXd instR, VectorXd instL);
    bool modifyCrash(MatrixXd &measureMatrix, int num);
    bool switchHands(MatrixXd &measureMatrix, int num);
    bool waitAndMove(MatrixXd &measureMatrix, int num);
    bool moveAndWait(MatrixXd &measureMatrix, int num);
    bool deleteInst(MatrixXd &measureMatrix, int num);

    //////////////////////////////////// IK
    VectorXd solveGeometricIK(VectorXd &pR, VectorXd &pL, double theta0, double theta7, double theta8, bool printError);
    double getLength(double theta);
    double getTheta(double l1, double theta);

};