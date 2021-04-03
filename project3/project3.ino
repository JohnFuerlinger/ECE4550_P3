#include "scheduler.h"


/** Define desired baud below **/
#define BAUDRATE					921600 /* highest typical baud */


/** Make this either 1 or 2, for the 2 task sets the project description gives us **/
#define RUN_TASKSET					2

TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xHandle3 = NULL;
TaskHandle_t xHandle4 = NULL;



// the loop function runs over and over again forever
void loop() {}

static void testFunc1( void *pvParameters )
{
	(void) pvParameters;
	
	#if PRINT_TASK_DEBUG
		char msgBuf[50];
		sprintf(msgBuf, "(TASKCODE) Starting T1, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);
	#endif
	
	/** Will force the task to execute for exactly its WCET and then 
	 ** immediately return. The below "loop" values were found using
	 ** trial and error. Note that they assume a 921600 baud and will
	 ** likely not be accurate if using a different baud.
	 **/
	#if (RUN_TASKSET == 1)
		#define T1_LOOPS	6800 /* 100 ms */
	#elif (RUN_TASKSET == 2)
		#define T1_LOOPS	6800 /* 100 ms */
	#else
		#error Valid values for RUN_TASKSET are either 1 or 2
	#endif
	int cnt = 0;
	while (1) {
		if (cnt % 1000 == 0) {
			Serial.println("1");
		}
		cnt++;
		if (cnt > T1_LOOPS) { break; }
	}
	
	#if PRINT_TASK_DEBUG
		sprintf(msgBuf, "(TASKCODE) Finishing T1, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);		
	#endif
}

static void testFunc2( void *pvParameters )
{
	(void) pvParameters;
	
	#if PRINT_TASK_DEBUG
		char msgBuf[50];
		sprintf(msgBuf, "(TASKCODE) Starting T2, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);
	#endif
	
	/** Will force the task to execute for exactly its WCET and then 
	 ** immediately return. The below "loop" values were found using
	 ** trial and error. Note that they assume a 921600 baud and will
	 ** likely not be accurate if using a different baud.
	 **/
	#if (RUN_TASKSET == 1)
		#define T2_LOOPS	14000 /* 200 ms */
	#elif (RUN_TASKSET == 2)
		#define T2_LOOPS	10000 /* 150 ms */
	#else
		#error Valid values for RUN_TASKSET are either 1 or 2
	#endif
	int cnt = 0;
	while (1) {
		if (cnt % 1000 == 0) {
			Serial.println("2");
		}
		cnt++;
		if (cnt > T2_LOOPS) { break; }
	}
	
	#if PRINT_TASK_DEBUG
		sprintf(msgBuf, "(TASKCODE) Finishing T2, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);		
	#endif
}

static void testFunc3( void *pvParameters )
{
	(void) pvParameters;
	
	#if PRINT_TASK_DEBUG
		char msgBuf[50];
		sprintf(msgBuf, "(TASKCODE) Starting T3, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);
	#endif
	
	/** Will force the task to execute for exactly its WCET and then 
	 ** immediately return. The below "loop" values were found using
	 ** trial and error. Note that they assume a 921600 baud and will
	 ** likely not be accurate if using a different baud.
	 **/
	#if (RUN_TASKSET == 1)
		#define T3_LOOPS	10000 /* 150 ms */
	#elif (RUN_TASKSET == 2)
		#define T3_LOOPS	14000 /* 200 ms */
	#else
		#error Valid values for RUN_TASKSET are either 1 or 2
	#endif
	int cnt = 0;
	while (1) {
		if (cnt % 1000 == 0) {
			Serial.println("3");
		}
		cnt++;
		if (cnt > T3_LOOPS) { break; }
	}
	
	#if PRINT_TASK_DEBUG
		sprintf(msgBuf, "(TASKCODE) Finishing T3, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);		
	#endif
}

static void testFunc4( void *pvParameters )
{
	(void) pvParameters;
	
	#if PRINT_TASK_DEBUG
		char msgBuf[50];
		sprintf(msgBuf, "(TASKCODE) Starting T4, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);
	#endif
	
	/** Will force the task to execute for exactly its WCET and then 
	 ** immediately return. The below "loop" values were found using
	 ** trial and error. Note that they assume a 921600 baud and will
	 ** likely not be accurate if using a different baud.
	 **/
	#if (RUN_TASKSET == 1)
		#define T4_LOOPS	20500 /* 300 ms */
	#elif (RUN_TASKSET == 2)
		#define T4_LOOPS	10000 /* 150 ms */
	#else
		#error Valid values for RUN_TASKSET are either 1 or 2
	#endif
	int cnt = 0;
	while (1) {
		if (cnt % 1000 == 0) {
			Serial.println("4");
		}
		cnt++;
		if (cnt > T4_LOOPS) { break; }
	}
	
	#if PRINT_TASK_DEBUG
		sprintf(msgBuf, "(TASKCODE) Finishing T4, t=%d", xTaskGetTickCount());
		Serial.println(msgBuf);		
	#endif
}

int main( void )
{
	char c1 = 'a';
	char c2 = 'b';
	char c3 = 'c';
	char c4 = 'd';
	/** Want baud as high as possible so we can use print statements
	 ** more without screwing up the RTOS. The arduino serial monitor
	 ** will not respect special chars in the terminal such as \010
	 ** for backspace, etc. Therefore all of my testing is done in 
	 ** TeraTerm, which is a more sophisticated Serial Terminal application.
	 **/
	Serial.begin(BAUDRATE);

	vSchedulerInit();
	
	Serial.print("400 ms = ");
	Serial.print(pdMS_TO_TICKS(400));
	Serial.println("ticks.");
	Serial.print("200 ms = ");
	Serial.print(pdMS_TO_TICKS(200));
	Serial.println("ticks.");
	Serial.print("700 ms = ");
	Serial.print(pdMS_TO_TICKS(700));
	Serial.println("ticks.");
	Serial.print("1000 ms = ");
	Serial.print(pdMS_TO_TICKS(1000));
	Serial.println("ticks.");
		
	#if (RUN_TASKSET == 1)
		vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, 1, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(400), pdMS_TO_TICKS(100), pdMS_TO_TICKS(400), 3);
		vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, 2, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(700), pdMS_TO_TICKS(200), pdMS_TO_TICKS(700), 6);
		vSchedulerPeriodicTaskCreate(testFunc3, "t3", configMINIMAL_STACK_SIZE, &c3, 3, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(1000), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1000), 8);
		vSchedulerPeriodicTaskCreate(testFunc4, "t4", configMINIMAL_STACK_SIZE, &c4, 4, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(5000), pdMS_TO_TICKS(300), pdMS_TO_TICKS(5000), 10);
	#elif (RUN_TASKSET == 2)
		vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, 1, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(400), pdMS_TO_TICKS(100), pdMS_TO_TICKS(400), 3);
		vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, 2, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS(150), pdMS_TO_TICKS(200), 6);
		vSchedulerPeriodicTaskCreate(testFunc3, "t3", configMINIMAL_STACK_SIZE, &c3, 3, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(700), pdMS_TO_TICKS(200), pdMS_TO_TICKS(700), 8);
		vSchedulerPeriodicTaskCreate(testFunc4, "t4", configMINIMAL_STACK_SIZE, &c4, 4, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(1000), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1000), 10);		
	#else
		#error Valid values for RUN_TASKSET are either 1 or 2
	#endif

	vSchedulerStart();

	/** We should never hit the below lines **/
	while(1) {
		Serial.println("ERROR: vSchedulerStart has returned.");
	}
}

