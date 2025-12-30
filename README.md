# LeShuffler
This project is made for the ARM Cortex-M7 STM32 platform (using the STM32H7).

## Description
In this project, the peripherials which are configured are Motors(Stepper Motor, DC Motor and Servo Motor), Sensors(IR, HallEffect and Proximity Sensor), External Memory, Buzzer and LCD Display. 

### Functions
#### Stepper Motor
-The TMC2209init is for the Stepper Motor. In this function the user has to set the TMC2209 handle, slaveAddress, UART handle, Timer handle 
```TMC2209init()```
-To run the stepper motor the user have to make the GPIO in pin output to a low level for **ENN** and **STANDBY** pin.
-The **MS1** and **MS2** pin output level is set according to the Microstepping is set. To set the different values of **MS1** and **MS2** refer to the datasheet. 
-The below function is to Set the microstep. 
```TMC2209_SetMicroSteps()```
-The below function is to Set the Driection as ClockWise or AntiClockWise of Motor
```TMC2209_SetDirection()```
-The below function is to Set PWM frequency in kHz
```SetPWMFrequency()```
-The below function is to Set the Step Count
```TMC2209_SetStepCount()```

#### DC Motor
-The DCmotor_Init is for the DC Motor. In this function the user has to set the DCmotor handle, Timer handle 
```DCmotor_Init()```
-This function is to Run the DC Motor and Solenoid, the user has to set the Direction and duty Cycle of the motor.
```setSolenoid()```
```SetMotor2Rotation()```
```SetMotor1Rotation()```

#### Servo Motor 
-The servoMotorInit is for the Servo Motor. In this function the user has to set the Servo Motor handle, Timer handle, Timer Channel
```servoMotorInit()```
-This function is to run the Servo Motor, the user has to set the Servo motor handle sturucture defination and angle. 
```ServoMotor_SetAngle()```

#### Sensor 
-In the below sensor function the user have to set the GPIO port and GPIO Pin number. 
```Proximity_Sensor()```
```HallEffect_Sensor()```
```IRSensor()```

### LCD Display
-To ON the blacklight of the LCD Display Set the GPIO pin output to a high level. 
-The below function will initialize the LCD (ili9488)
```BSP_LCD_Init()```
-The below function is to make the LCD screen Clear, the user has to set the display color
```BSP_LCD_Clear()```

-The below function is to initialize the External Memory configuartions. 
```W25Q128_OCTO_SPI_Init()```
-The below function is the write function for the external Memory at the particular address in a flash. In STM32H7B0VBT6 the flash Memory address is **0x70000000**.
-The user have to put the image data, starting address and size of the image. 
```W25Q128_OSPI_Write()```
-The below function is to enable the Memory Mapped Mode for the External Memory. 
```W25Q128_OSPI_EnableMemoryMappedMode()```
-The below function is to Read the ID of the ili9488. 
```ili9488_ReadID()```
-The below functyion is for the Custom Font. The user have to set teh font, x and y position, text, text color and Background color.
```tftstDrawTextWithFont();```
-The below function is to draw the RGB 8 bit Image. The user have to set the x and y position, image height and width, and address. 
```ili9488_DrawRGBImage8bit();```


#### Note on debouncing read_sensor.
Used in:
	wait_sensor() 			debounced
	wait_seen_entry()		debounced
	cards_in_tray()			debounced
	cards_in_shoe()			debounced
	load_one_card()			out of function debouncing 
	load_max_n_cards() 		external debouncing of load_one_card()
	load_double_deck() 		external debouncing of load_one_card()
	crsl_at_home()			cannot be debounced, time-critical
	slot_light()			external debouncing except in tests
	slot_dark()				external debouncing except in tests
	home_carousel()			initial card check x 2 with debouncing
	carousel_vibration()	x2 no debouncing
	eject_one_card()		x4 no debouncing, used only for troubleshooting (wiggle, vibration)
	display_sensors()		NA
	testStep()				test only
	




