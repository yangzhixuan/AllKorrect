################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/AllKorrect.cpp \
../src/Daemon.cpp \
../src/Execute.cpp \
../src/FileSystem.cpp \
../src/Log.cpp \
../src/Message.cpp 

OBJS += \
./src/AllKorrect.o \
./src/Daemon.o \
./src/Execute.o \
./src/FileSystem.o \
./src/Log.o \
./src/Message.o 

CPP_DEPS += \
./src/AllKorrect.d \
./src/Daemon.d \
./src/Execute.d \
./src/FileSystem.d \
./src/Log.d \
./src/Message.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


