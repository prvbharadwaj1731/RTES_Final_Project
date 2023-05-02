################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
drivers/%.obj: ../drivers/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccs1220/ccs/tools/compiler/ti-cgt-arm_20.2.7.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=FPv4SPD16 --abi=eabi -me --include_path="C:/ti/ccs1220/ccs/tools/compiler/ti-cgt-arm_20.2.7.LTS/include" --include_path="C:/Users/prvbh/OneDrive/Desktop/Private/Masters/fourth_semester/RTES/Final_project/ADC_test/FreeRTOS/portable/CCS/ARM_CM4F" --include_path="C:/Users/prvbh/OneDrive/Desktop/Private/Masters/fourth_semester/RTES/Final_project/ADC_test" --include_path="C:/Users/prvbh/OneDrive/Desktop/Private/Masters/fourth_semester/RTES/Final_project/ADC_test/driverlib" --include_path="C:/Users/prvbh/OneDrive/Desktop/Private/Masters/fourth_semester/RTES/Final_project/ADC_test/FreeRTOS/include" --include_path="C:/Users/prvbh/OneDrive/Desktop/Private/Masters/fourth_semester/RTES/Final_project/ADC_test/FreeRTOS/portable/CCS/ARM_CM4F" -g --gcc --define=ccs="ccs" --define=TARGET_IS_TM4C129_RA1 --define=PART_TM4C1294NCPDT --define=DEBUG --display_error_number --diag_wrap=off --diag_warning=225 --preproc_with_compile --preproc_dependency="drivers/$(basename $(<F)).d_raw" --obj_directory="drivers" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


