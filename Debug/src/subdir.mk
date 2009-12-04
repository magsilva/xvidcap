################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/app_data.o \
../src/capture.o \
../src/codecs.o \
../src/colors.o \
../src/dbus-server-object.o \
../src/eggtrayicon.o \
../src/frame.o \
../src/gnome_frame.o \
../src/gnome_options.o \
../src/gnome_ui.o \
../src/gnome_warning.o \
../src/job.o \
../src/led_meter.o \
../src/main.o \
../src/options.o \
../src/xtoffmpeg.o \
../src/xtoxwd.o \
../src/xvc_error_item.o \
../src/xvidcap-dbus-client.o 

C_SRCS += \
../src/app_data.c \
../src/capture.c \
../src/codecs.c \
../src/colors.c \
../src/dbus-server-object.c \
../src/eggtrayicon.c \
../src/frame.c \
../src/gnome_frame.c \
../src/gnome_options.c \
../src/gnome_ui.c \
../src/gnome_warning.c \
../src/job.c \
../src/led_meter.c \
../src/main.c \
../src/options.c \
../src/xtoffmpeg.c \
../src/xtoxwd.c \
../src/xvc_error_item.c \
../src/xvidcap-dbus-client.c 

OBJS += \
./src/app_data.o \
./src/capture.o \
./src/codecs.o \
./src/colors.o \
./src/dbus-server-object.o \
./src/eggtrayicon.o \
./src/frame.o \
./src/gnome_frame.o \
./src/gnome_options.o \
./src/gnome_ui.o \
./src/gnome_warning.o \
./src/job.o \
./src/led_meter.o \
./src/main.o \
./src/options.o \
./src/xtoffmpeg.o \
./src/xtoxwd.o \
./src/xvc_error_item.o \
./src/xvidcap-dbus-client.o 

C_DEPS += \
./src/app_data.d \
./src/capture.d \
./src/codecs.d \
./src/colors.d \
./src/dbus-server-object.d \
./src/eggtrayicon.d \
./src/frame.d \
./src/gnome_frame.d \
./src/gnome_options.d \
./src/gnome_ui.d \
./src/gnome_warning.d \
./src/job.d \
./src/led_meter.d \
./src/main.d \
./src/options.d \
./src/xtoffmpeg.d \
./src/xtoxwd.d \
./src/xvc_error_item.d \
./src/xvidcap-dbus-client.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


