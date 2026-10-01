################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/ui/components/ui_comp_hook.c 

OBJS += \
./Drivers/ui/components/ui_comp_hook.o 

C_DEPS += \
./Drivers/ui/components/ui_comp_hook.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/ui/components/%.o Drivers/ui/components/%.su Drivers/ui/components/%.cyclo: ../Drivers/ui/components/%.c Drivers/ui/components/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I"E:/STM32CubeIDE/F446RET6 Streamdeck/Drivers" -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers/ui/screens" -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers/ui" -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers/lvgl" -I"E:/STM32CubeIDE/F446RET6 Streamdeck/Drivers/lvgl/src/drivers/display/st7789" -I"E:/STM32CubeIDE/F446RET6 Streamdeck/Drivers/lvgl" -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I"E:/STM32CubeIDE/F446RET6-Streamdeck-main/F446RET6-Streamdeck-main/Drivers" -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/HID/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-ui-2f-components

clean-Drivers-2f-ui-2f-components:
	-$(RM) ./Drivers/ui/components/ui_comp_hook.cyclo ./Drivers/ui/components/ui_comp_hook.d ./Drivers/ui/components/ui_comp_hook.o ./Drivers/ui/components/ui_comp_hook.su

.PHONY: clean-Drivers-2f-ui-2f-components

