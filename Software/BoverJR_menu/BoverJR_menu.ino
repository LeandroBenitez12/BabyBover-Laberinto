// libreries
#include <PID.h>
#include <EngineController.h>
#include <DistanceSensors.h>
#include <Button.h>
#include "BluetoothSerial.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

// I2C
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

// BLUETOOH
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;

// DEBUG
#define DEBUG_BUTTON 0
#define DEBUG_STATUS 0
#define DEBUG_SENSORS 0
#define DEBUG_PID 0
#define DEBUG_EJE_Z 0
// TICKS DEBUG
#define TICK_DEBUG_ALL 500
unsigned long currentTimeDebugAll = 0;

// PINOUT
// Motores
#define PIN_RIGHT_ENGINE_IN1 27
#define PIN_RIGHT_ENGINE_IN2 26
#define PIN_LEFT_ENGINE_IN1 19
#define PIN_LEFT_ENGINE_IN2 18
#define PWM_CHANNEL_RIGHT_IN1 1
#define PWM_CHANNEL_RIGHT_IN2 2
#define PWM_CHANNEL_LEFT_IN1 3
#define PWM_CHANNEL_LEFT_IN2 4

// Sharps
#define PIN_SHARP_RIGHT 25
#define PIN_SHARP_LEFT 35
#define PIN_SHARP_FRONT 33
float rightDistance;
float leftDistance;
float frontDistance;
#define PARED_ENFRENTE 10
#define PARED_COSTADO_PASILLO 18
#define NO_HAY_PARED 24
#define NO_HAY_PARED_ENFRENTE 24

// MPU
#define INTERRUPT_PIN 4
#define MARGEN 5
#define POSITIVE_ANGLE_MAX 180
#define RIGHT_LIMIT_NEG -180
unsigned long currentTimewhileZ;
float gyroZ;

// Boton
#define PIN_BUTTON_START 32
bool stateStartButton = 0;

// MENU variables
#define VALUE_5 5     // valor 5 para aumentar/disminuir velocidad
#define VALUE_0_1 0.1 // valor 0.1 para aumentar/disminuir PID
bool parar_motores = false;
bool iniciarRobot = false;
bool walltofollow = false;
unsigned long currentTimeMenu = 0;

// veocidades motores pwm
#define SPEED_TURN_LOW 90
#define ENTRAR_EN_PASILLO 300
#define OMITIR_BIFURCACION 600
#define MAX_SPEED 255
int averageSpeedRight = 140; // velocidad inicial
int averageSpeedLeft = 150;  // velocidad inicial + 10
int speedRightPID;
int speedLeftPID;
int speedRightPID2;
int speedLeftPID2;

// Constantes y variables pid
unsigned long currentTimePID = 0;
#define TICK_PID 1
double kp = 3;
double kd = 0.6;
double ki = 0.0;
double setPoint = 0.0;
double gananciaPID = 0;
double input = 0;

// variables pid2
double kp2 = 0;
double kd2 = 0;
double ki2 = 0.0;
double setPoint2 = 0;
double gananciaPID2;
double TICK_PID2 = 1.0;

unsigned long currentTimeStop = 0;
#define TICK_STOP 1000
#define TICK_CENTRAR 500
// enum state
enum movement
{
  STANDBY,
  CONTINUE,
  STOP,
  RIGHT_TURN,
  LEFT_TURN,
  FULL_TURN,
  POST_TURN_FULL,
  POST_TURN_MIDDLE,
  ANT_TURN
};
int movement = STANDBY;

// INSTANCIANDO OBJETOS
IEngine *leftEngine = new Driver_DRV8825(PIN_RIGHT_ENGINE_IN1, PIN_RIGHT_ENGINE_IN2, PWM_CHANNEL_RIGHT_IN1, PWM_CHANNEL_RIGHT_IN2);
IEngine *rightEngine = new Driver_DRV8825(PIN_LEFT_ENGINE_IN1, PIN_LEFT_ENGINE_IN2, PWM_CHANNEL_LEFT_IN1, PWM_CHANNEL_LEFT_IN2);
EngineController *Bover = new EngineController(rightEngine, leftEngine);
Isensor *SharpRight = new Sharp_GP2Y0A21(PIN_SHARP_RIGHT);
Isensor *SharpLeft = new Sharp_GP2Y0A21(PIN_SHARP_LEFT);
Isensor *SharpFront = new Sharp_GP2Y0A21(PIN_SHARP_FRONT);
Pid *PID = new Pid(kp, kd, ki, setPoint, TICK_PID);
Pid *PID2 = new Pid(kp2, kd2, ki2, setPoint2, TICK_PID2);
Button *buttonStart1 = new Button(PIN_BUTTON_START);
MPU6050 mpu;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
Quaternion q;           // [w, x, y, z]
VectorInt16 aa;         // [x, y, z]
VectorInt16 aaReal;     // [x, y, z]
VectorInt16 aaWorld;    // [x, y, z]
VectorFloat gravity;    // [x, y, z]
float ypr[3];           // [yaw, pitch, roll]
volatile bool mpuInterrupt = false;

void dmpDataReady()
{
  mpuInterrupt = true;
}

void mpuSetup()
{
// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  Wire.begin();
  Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif

  // Iniciar MPU6050
  SerialBT.println(F("Initializing I2C devices..."));
  mpu.initialize();
  pinMode(INTERRUPT_PIN, INPUT);

  // Comprobar  conexion
  SerialBT.println(F("Testing device connections..."));
  SerialBT.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

  // Iniciar DMP
  SerialBT.println(F("Initializing DMP..."));
  devStatus = mpu.dmpInitialize();

  // Valores de calibracion
  mpu.setXGyroOffset(60);
  mpu.setYGyroOffset(49);
  mpu.setZGyroOffset(-69);
  mpu.setZAccelOffset(1046);

  // Activar DMP
  if (devStatus == 0)
  {
    SerialBT.println(F("Enabling DMP..."));
    mpu.setDMPEnabled(true);

    // Activar interrupcion
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();

    SerialBT.println(F("DMP ready! Waiting for first interrupt..."));
    dmpReady = true;

    // get expected DMP packet size for later comparison
    packetSize = mpu.dmpGetFIFOPacketSize();
  }
  else
  {
    // ERROR!
    // 1 = initial memory load failed
    // 2 = DMP configuration updates failed
    // (if it's going to break, usually the code will be 1)
    SerialBT.print(F("DMP Initialization failed (code "));
    SerialBT.print(devStatus);
    SerialBT.println(F(")"));
  }
}
// MPU LOOP
float mpuLoop()
{
  // Si fallo al iniciar, parar programa
  // if (!dmpReady)
  // return;
  // Ejecutar mientras no hay interrupcion
  while (!mpuInterrupt && fifoCount < packetSize)
  {
  // AQUI EL RESTO DEL CODIGO DE TU PROGRRAMA
  }
  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();

  // Obtener datos del FIFO
  fifoCount = mpu.getFIFOCount();

  // Controlar overflow
  if ((mpuIntStatus & 0x10) || fifoCount == 4096)
  {
    mpu.resetFIFO();
    // SerialBT.println(F("FIFO overflow!"));
  }
  else if (mpuIntStatus & 0x02)
  {
    // wait for correct available data length, should be a VERY short wait
    while (fifoCount < packetSize)
      fifoCount = mpu.getFIFOCount();

    // read a packet from FIFO
    mpu.getFIFOBytes(fifoBuffer, packetSize);

    // track FIFO count here in case there is > 1 packet available
    // (this lets us immediately read more without waiting for an interrupt)
    fifoCount -= packetSize;

    // MMostrar Yaw, Pitch, Roll
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    float gz;
    return gz = ypr[0] * 180 / M_PI;
    delay(1);
  }
}
// LECTURA SENSORS
void SensorsRead()
{
  frontDistance = SharpFront->SensorRead();
  rightDistance = SharpRight->SensorRead();
  leftDistance = SharpLeft->SensorRead();
}

// IMPRIMIR EN BLUETOOH
void printButton()
{
  SerialBT.print("Button Start: ");
  SerialBT.println(stateStartButton);
}

void printPID()
{
  if (millis() > currentTimePID + TICK_DEBUG_ALL)
  {
    currentTimePID = millis();
    SerialBT.println("");
    SerialBT.print("Ganancia PID 1: ");
    SerialBT.println(gananciaPID);
    // SerialBT.print("Ganancia PID 2: ");
    // SerialBT.println(gananciaPID2);
    SerialBT.print("speedRight: ");
    SerialBT.print(speedRightPID);
    SerialBT.print(" || speedLeft: ");
    SerialBT.println(speedLeftPID);
    SerialBT.println("");
  }
}

void printEjeZ()
{
  SerialBT.print("Eje Z:  ");
  SerialBT.print(gyroZ);
}

void printSensors()
{
  SerialBT.print("LeftDistance: ");
  SerialBT.println(leftDistance);
  SerialBT.print("frontDistance: ");
  SerialBT.println(frontDistance);
  SerialBT.print(" || rightDistance: ");
  SerialBT.println(rightDistance);
}

void printOptions()
{
  // clean the serial
  for (int i = 0; i < 5; i++)
  {
    SerialBT.println("");
  }
  SerialBT.println("Configuracion Actual:");

  SerialBT.print("A+/B- KP = ");
  SerialBT.println(kp);

  SerialBT.print("C+/D- KP2 = ");
  SerialBT.println(kp2);

  SerialBT.print("E+/F- KD = ");
  SerialBT.println(kd);

  SerialBT.print("G+/H- KD2 = ");
  SerialBT.println(kd2);

  SerialBT.print("I+/J- - leftSpeed = ");
  SerialBT.println(averageSpeedLeft);

  SerialBT.print("K+/L- - rightLeft = ");
  SerialBT.println(averageSpeedRight);

  SerialBT.print("N - Wall to follow = ");
  String state = "";
  if (walltofollow == true)
    state = "Right";
  else
    state = "Left";
  SerialBT.println(state);

  SerialBT.print("P - Motor parado = ");
  SerialBT.println(parar_motores);

  SerialBT.println("Y - DEBUGEAR ROBOT ");

  SerialBT.println("Z - INICIAR ROBOT ");

  for (int i = 0; i < 5; i++)
  {
    SerialBT.println("");
  }
}
// GIROS 90º Y 180º
void turnRight()
{
  SerialBT.println("  entro right  ");
  do
  {
    SensorsRead();
    Bover->Right(SPEED_TURN_LOW, SPEED_TURN_LOW);
  } while ( frontDistance < 28);
  SerialBT.println("  sali right  ");
  movement = POST_TURN_MIDDLE;
}

void turnLeft()
{
  SerialBT.println("  entro left  ");
  do
  {
    SensorsRead();
    Bover->Left(SPEED_TURN_LOW, SPEED_TURN_LOW);
  } while ( frontDistance < 28);
  SerialBT.println("  sali left  ");
  movement = POST_TURN_MIDDLE;
}

void fullTurn()
{
  SerialBT.println("  entro full  ");
  do
  {
    SensorsRead();
    Bover->Right(SPEED_TURN_LOW, SPEED_TURN_LOW);
  } while ( frontDistance < 28);
  SerialBT.println("  sali FULL  ");
  movement = CONTINUE;
}

void postTurnFull()
{
  Bover->Forward(averageSpeedRight, averageSpeedLeft);
  delay(OMITIR_BIFURCACION);
}
void postTurnMiddle()
{
  Bover->Forward(averageSpeedRight, averageSpeedLeft);
  delay(ENTRAR_EN_PASILLO);
}

void antTurn()
{
  Bover->Forward(averageSpeedRight, averageSpeedLeft);
  delay(TICK_CENTRAR);
}

void menuBT()
{
  if (SerialBT.available())
  {
    char option = SerialBT.read();
    switch (option)
    {
    case 'M':
    {
      printOptions();
      break;
    }
    case 'A':
    {
      kp += VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'B':
    {
      kp -= VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'C':
    {
      kp2 += VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'D':
    {
      kp2 -= VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'E':
    {
      kd += VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'F':
    {
      kd -= VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'G':
    {
      kd2 += VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'H':
    {
      kd2 -= VALUE_0_1;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'I':
    {
      averageSpeedLeft = averageSpeedLeft + VALUE_5;

      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'J':
    {
      averageSpeedLeft = averageSpeedLeft - VALUE_5;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'K':
    {
      averageSpeedRight = averageSpeedRight + VALUE_5;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }

    case 'L':
    {
      averageSpeedRight -= VALUE_5;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'N':
    {
      walltofollow = true;

      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'P':
    {
      parar_motores = true;

      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    case 'Z':
    {
      stateStartButton = true;
      SerialBT.println("");
      printOptions();
      SerialBT.println("");
      break;
    }
    default:
      SerialBT.println("OPCION INCORRECTA ");
      break;
    }
  }
}

void movementLogic()
{
  switch (movement)
  {
  case STANDBY:
  {
    Bover->Stop();
    if (stateStartButton)
    {
      delay(2000);
      movement = CONTINUE;
    }
  }
  break;
  case CONTINUE:
  {
    //float input = leftDistance;
    //if (walltofollow)
    //  input = rightDistance;

    input = rightDistance - leftDistance;
    gananciaPID = PID->ComputePid(input);
    // gananciaPID2 = PID2->ComputePid(input2);
    //  INPUT2 = WALLTOFOLLOW PARA MANTENER LA DISTANCIA A ESA PARED
    //  float input2 = leftDistance;
    //  if (walltofollow == true)
    //  input2 = rightDistance;
    //  gananciaPID2 = PID2->ComputePid(input2);

    if (DEBUG_PID)
      printPID();

    speedRightPID = (averageSpeedRight - (gananciaPID));
    speedLeftPID = (averageSpeedLeft + (gananciaPID));

    // APLICAMOS GANANCIA 1 DEL PID A MOTORES. okey
    // if (walltofollow)
    // {
    //   speedRightPID = (averageSpeedRight - (gananciaPID));
    //   speedLeftPID = (averageSpeedLeft + (gananciaPID));
    // }
    // else
    // {
    //   speedRightPID = (averageSpeedRight + (gananciaPID));
    //   speedLeftPID = (averageSpeedLeft - (gananciaPID));
    // }
    // APLICAMOS GANANCIA 2 DEL PID A MOTORES
    // speedRightPID2 = (speedRightPID + (gananciaPID2));
    // speedLeftPID2 = (speedLeftPID - (gananciaPID2));

    // ESTABLECEMOS LOS LIMITES
    if (speedLeftPID > MAX_SPEED)
    {
      int umbralLeft = (speedLeftPID - MAX_SPEED);
      speedRightPID = speedRightPID - umbralLeft;
      speedLeftPID = MAX_SPEED;
    }

    if (speedRightPID > MAX_SPEED)
    {
      int umbralRight = (speedRightPID - MAX_SPEED);
      speedLeftPID = speedLeftPID - umbralRight;
      speedRightPID = MAX_SPEED;
    }

    if (parar_motores)
    {
      Bover->Stop();
    }
    else
    {
      Bover->Forward(speedRightPID, speedLeftPID);
    }

    if (walltofollow == true)
    {
      if (leftDistance > NO_HAY_PARED && frontDistance > NO_HAY_PARED_ENFRENTE && rightDistance > NO_HAY_PARED)
      {
        movement = ANT_TURN;
      }
      if (leftDistance <= PARED_COSTADO_PASILLO && frontDistance > NO_HAY_PARED_ENFRENTE && rightDistance > NO_HAY_PARED)
      {
        movement = ANT_TURN;
      }
      if (leftDistance > NO_HAY_PARED && frontDistance > NO_HAY_PARED_ENFRENTE && rightDistance <= PARED_COSTADO_PASILLO)
      {
        movement = POST_TURN_FULL;
      }
    }
    else
    {
      if (leftDistance > NO_HAY_PARED && frontDistance > NO_HAY_PARED_ENFRENTE && rightDistance > NO_HAY_PARED)
      {
        movement = ANT_TURN;
      }
      if (leftDistance > NO_HAY_PARED && frontDistance > NO_HAY_PARED_ENFRENTE && rightDistance <= PARED_COSTADO_PASILLO)
      {
        movement = ANT_TURN;
      }
      if (leftDistance <= PARED_COSTADO_PASILLO && frontDistance > NO_HAY_PARED_ENFRENTE && rightDistance > NO_HAY_PARED)
      {
        movement = POST_TURN_FULL;
      }
    }
    if (frontDistance <= PARED_ENFRENTE)
    {
      movement = STOP;
    }
    break;
  }
  case STOP:
  {
    Bover->Stop();
    delay(500);
    if (frontDistance <= PARED_ENFRENTE)
    {
      if (rightDistance > NO_HAY_PARED && leftDistance > NO_HAY_PARED)
      {
        if (walltofollow = true)
        {
          movement = RIGHT_TURN;
        }
        else
        {
          movement = LEFT_TURN;
        }
      }
      else if (rightDistance <= PARED_COSTADO_PASILLO && leftDistance <= PARED_COSTADO_PASILLO)
      {
        movement = FULL_TURN;
      }
      else if (rightDistance > NO_HAY_PARED && leftDistance <= PARED_COSTADO_PASILLO)
      {
        movement = RIGHT_TURN;
      }
      else if (rightDistance <= PARED_COSTADO_PASILLO && leftDistance > NO_HAY_PARED)
      {
        movement = LEFT_TURN;
      }
    }
    else
    {
      movement = CONTINUE;
    }
    break;
  }

  case RIGHT_TURN:
  {
    turnRight();
    break;
  }

  case LEFT_TURN:
  {
    turnLeft();
    break;
  }

  case FULL_TURN:
  {
    fullTurn();
    break;
  }

  case POST_TURN_MIDDLE:
  {
    postTurnMiddle();
    movement = CONTINUE;
    break;
  }

  case POST_TURN_FULL:
  {
    postTurnFull();
    movement = CONTINUE;
    break;
  }
  case ANT_TURN:
  {
    antTurn();
    if (walltofollow == true)
    {
      movement = RIGHT_TURN;
    }
    else
    {
      movement = LEFT_TURN;
    }
  }
  }
}

void printStatus()
{
  String state = "";
  switch (movement)
  {
  case STANDBY:
    state = "STANDBY";
    break;
  case CONTINUE:
    state = "CONTINUE";
    break;
  case STOP:
    state = state = "STOP";
    break;
  case RIGHT_TURN:
    state = "RIGHT TURN";
    break;
  case LEFT_TURN:
    state = "LEFT TURN";
    break;
  case FULL_TURN:
    state = "FULL TURN";
    break;
  case POST_TURN_MIDDLE:
    state = "POST TURN MIDDLE";
    break;
  case POST_TURN_FULL:
    state = "POST TURN FULL";
    break;
  case ANT_TURN:
    state = "ANT TURN ";
    break;
  }
  SerialBT.print("State: ");
  SerialBT.println(state);
}

void printAll()
{
  if (millis() > currentTimeDebugAll + TICK_DEBUG_ALL)
  {
    currentTimeDebugAll = millis();
    if (DEBUG_BUTTON)
      printButton();
    SerialBT.println("");
    if (DEBUG_SENSORS)
      printSensors();
    SerialBT.println("");
    if (DEBUG_STATUS)
      printStatus();
    SerialBT.println("");
    if (DEBUG_EJE_Z)
      printEjeZ();
    SerialBT.println("");
  }
}
void setup()
{
  Serial.begin(115200);
  SerialBT.begin("Bover JR");
  mpuSetup();
}

void loop()
{
  gyroZ = mpuLoop();
  stateStartButton = buttonStart1->GetIsPress();
  SensorsRead();
  if (iniciarRobot == false)
  {
    menuBT();
  }
  if (stateStartButton == true)
  {
    iniciarRobot = true;
  }
  if (iniciarRobot == true)
  {
    movementLogic();
    printAll();
  }
}