# [Jetson AGX Orin용 통합 Makefile]

CC = g++

# 1. 컴파일 옵션
# OpenCV 경로는 pkg-config로 잡음 (Jetson 최적화 버전 자동 인식)
CFLAGS = -Wall -O2 -g -std=c++17 -fPIC $(shell pkg-config --cflags opencv4)

# 2. 헤더 경로 설정
# [중요] ./3rdparty/aruco/include 가 추가되어야 함.
INCLUDE = -I./include \
          -I./include/USBIO_advantech \
          -I./include/managers \
          -I./include/motors \
          -I./include/tasks \
          -I./include/usbio \
          -I./3rdparty/aruco/include

# 3. 라이브러리 링크 설정 (LDFLAGS)
# 라이브러리 위치 지정 (-L./lib) 및 런타임 경로 설정 (rpath)
LDFLAGS = -lm -lpthread -lstdc++fs -L./lib -Wl,-rpath,'$$ORIGIN/lib'

# [아키텍처 변경] PC용(x64) -> Jetson용(arm64) 라이브러리로 변경
LDFLAGS += -lUSBIO_arm64
LDFLAGS += -ldxl_sbc_cpp

# [참조 반영] Git에 있던 SFML 및 RealSense 추가
LDFLAGS += -lsfml-audio -lsfml-system
LDFLAGS += -lrealsense2

# [중요] OpenCV 라이브러리
# pkg-config 사용해야 Jetson의 HW 가속(GStreamer, CUDA 등) 의존성을 잡음.
# ★ 주의: -lopencv_aruco는 소스로 빌드
LDFLAGS += $(shell pkg-config --libs opencv4)

# 4. 소스 및 빌드 경로 설정
SRCDIR = ./src
ARUCO_SRCDIR = ./3rdparty/aruco/src
OBJDIR = ./obj
BINDIR = ./bin
EXECUTABLE = main.out

# 5. 소스 파일 목록
# (1) 내 프로젝트 소스
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJFILES := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))

# (2) ArUco 소스 (Git Makefile엔 없고 추가한 부분)
ARUCO_SOURCES := $(wildcard $(ARUCO_SRCDIR)/*.cpp)
ARUCO_OBJFILES := $(patsubst $(ARUCO_SRCDIR)/%.cpp, $(OBJDIR)/aruco/%.o, $(ARUCO_SOURCES))

# 6. 빌드 규칙
all: directories $(BINDIR)/$(EXECUTABLE)

directories:
	mkdir -p $(OBJDIR) $(BINDIR)
	mkdir -p $(OBJDIR)/aruco

# 실행 파일 생성 (내 오브젝트 + ArUco 오브젝트 + 라이브러리들)
$(BINDIR)/$(EXECUTABLE): $(OBJFILES) $(ARUCO_OBJFILES)
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE) $(LDFLAGS)

# 일반 소스 컴파일 규칙
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

# ArUco 소스 컴파일 규칙
$(OBJDIR)/aruco/%.o: $(ARUCO_SRCDIR)/%.cpp
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	rm -rf $(OBJDIR) $(BINDIR)/$(EXECUTABLE)

.PHONY: debug
debug: CFLAGS := -Wall -g3 -O0 -fno-omit-frame-pointer -std=c++17 -fPIC \
                 $(shell pkg-config --cflags opencv4)
debug: all

.PHONY: asan
asan: CFLAGS  := -Wall -g3 -O0 -fno-omit-frame-pointer -fsanitize=address \
                 -std=c++17 -fPIC $(shell pkg-config --cflags opencv4)
asan: LDFLAGS := $(LDFLAGS) -fsanitize=address
asan: all