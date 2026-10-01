################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/ui/screens/ui_Screen1.c \
../Drivers/ui/screens/ui_Screen2.c \
../Drivers/ui/screens/ui_Screen3.c \
../Drivers/ui/screens/ui_Screen4.c 

OBJS += \
./Drivers/ui/screens/ui_Screen1.o \
./Drivers/ui/screens/ui_Screen2.o \
./Drivers/ui/screens/ui_Screen3.o \
./Drivers/ui/screens/ui_Screen4.o 

C_DEPS += \
./Drivers/ui/screens/ui_Screen1.d \
./Drivers/ui/screens/ui_Screen2.d \
./Drivers/ui/screens/ui_Screen3.d \
./Drivers/ui/screens/ui_Screen4.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/ui/screens/%.o Drivers/ui/screens/%.su Drivers/ui/screens/%.cyclo: ../Drivers/ui/screens/%.c Drivers/ui/screens/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I"E:/STM32CubeIDE/F446RET6 Streamdeck/Drivers" -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers/ui/screens" -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers/ui" -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers/lvgl" -I"E:/STM32CubeIDE/F446RET6 Streamdeck/Drivers/lvgl/src/drivers/display/st7789" -I"E:/STM32CubeIDE/F446RET6 Streamdeck/Drivers/lvgl" -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers" -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/HID/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-ui-2f-screens

clean-Drivers-2f-ui-2f-screens:
	-$(RM) ./Drivers/ui/screens/ui_Screen1.cyclo ./Drivers/ui/screens/ui_Screen1.d ./Drivers/ui/screens/ui_Screen1.o ./Drivers/ui/screens/ui_Screen1.su ./Drivers/ui/screens/ui_Screen2.cyclo ./Drivers/ui/screens/ui_Screen2.d ./Drivers/ui/screens/ui_Screen2.o ./Drivers/ui/screens/ui_Screen2.su ./Drivers/ui/screens/ui_Screen3.cyclo ./Drivers/ui/screens/ui_Screen3.d ./Drivers/ui/screens/ui_Screen3.o ./Drivers/ui/screens/ui_Screen3.su ./Drivers/ui/screens/ui_Screen4.cyclo ./Drivers/ui/screens/ui_Screen4.d ./Drivers/ui/screens/ui_Screen4.o ./Drivers/ui/screens/ui_Screen4.su

.PHONY: clean-Drivers-2f-ui-2f-screens

