/*
 * MONITOREO ACÚSTICO
 * Autor: Eric Yu Yu
 * Tesis - Caminata sonora 
 * 
 */

#include <driver/i2s.h>
#include "sos-iir-filter.h"
#include "microfono.h"
#include <WiFi.h>
#include "time.h"
#include "fbase.h"
#include "ntpHora.h"

#define SEND_TIME     60000 //Envio cada 10m (60000)

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// FOURIER ////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "arduinoFFT.h" 
arduinoFFT FFT = arduinoFFT();

#define SAMPLES         512
double vReal[SAMPLES]={};
double vImag[SAMPLES]={};

int flag=0;
int flag2=0;


//////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// Sampling ////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Nivel de ruido equivalente
double Leq_dB = 0;
static double Leq_arr[SAMPLES];
static double Leq_arr_sort[SAMPLES];


#define SAMPLE_RATE       48000 // Hz
#define SAMPLE_BITS       32    // bits
#define SAMPLE_T          int32_t 
#define SAMPLES_SHORT     (SAMPLE_RATE / 8) // 6000muestras~125ms (48000Hz)
#define SAMPLES_LEQ       (SAMPLE_RATE * LEQ_PERIOD)
#define DMA_BANK_SIZE     (SAMPLES_SHORT / 16)
#define DMA_BANKS         32


struct sum_queue_t {
  // Suma de medidos (filtro)
  float sum_sqr_SPL;
  // Sum de cuadrados ponderados
  float sum_sqr_weighted;
  // Debug
  uint32_t proc_ticks;
};

QueueHandle_t samples_queue;

// Buffer (estático) de samples
float samples[SAMPLES_SHORT] __attribute__((aligned(4)));

void mic_i2s_init() {
  // Setup interfaz I2S --> Muestreo 32bits (INMP441)
  const i2s_config_t i2s_config = {
    mode: i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    sample_rate: SAMPLE_RATE,
    bits_per_sample: i2s_bits_per_sample_t(SAMPLE_BITS),
    channel_format: I2S_CHANNEL_FMT_ONLY_LEFT,
    communication_format: i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    intr_alloc_flags: ESP_INTR_FLAG_LEVEL1,
    dma_buf_count: DMA_BANKS,
    dma_buf_len: DMA_BANK_SIZE,
    use_apll: true,
    tx_desc_auto_clear: false,
    fixed_mclk: 0
  };
  
  /* I2S pines
  I2S_WS            21 
  I2S_SCK           19 
  I2S_SD            18 
  I2S_PORT          I2S_NUM_0*/
  
  const i2s_pin_config_t pin_config = {
    bck_io_num:   I2S_SCK,  
    ws_io_num:    I2S_WS,    
    data_out_num: -1, //-1 not used
    data_in_num:  I2S_SD   
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  #if (MIC_TIMING_SHIFT > 0) 
    REG_SET_BIT(I2S_TIMING_REG(I2S_PORT), BIT(9));   
    REG_SET_BIT(I2S_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);  
  #endif
  
  i2s_set_pin(I2S_PORT, &pin_config);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// INICIAR DB y WIFI ////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

Network *network;

void initNetwork(){
  network = new Network();
  network->initWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// I2S LECTURA (TASK 1) ////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mic_i2s_reader_task(void* parameter) {
  mic_i2s_init(); //inicialización

  size_t bytes_read = 0;
  i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);

  while (true) {
    i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(SAMPLE_T), &bytes_read, portMAX_DELAY);

    //TickType_t start_tick = xTaskGetTickCount();

    SAMPLE_T* int_samples = (SAMPLE_T*)&samples;
    for(int i=0; i<SAMPLES_SHORT; i++) samples[i] = MIC_CONVERT(int_samples[i]);

    sum_queue_t q;
        
    //Equalizacion por microfono (INMP441) - referencia de respuesta en frecuencia DATASHEET
    q.sum_sqr_SPL = MIC_EQUALIZER.filter(samples, samples, SAMPLES_SHORT);

    //Aplicacion de ponderacion
    q.sum_sqr_weighted = WEIGHTING.filter(samples, samples, SAMPLES_SHORT);

    // Debug
    //q.proc_ticks = xTaskGetTickCount() - start_tick;

    xQueueSend(samples_queue, &q, portMAX_DELAY);
  }
  vTaskDelete(NULL);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// Calculo (TASK 2) ////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

static double octave[8]={};
static double LAeq;
static double LA10;
static double LA90;
static double sum=0;
static double maximo;
static double minimo;

int cmpfunc (const void * a, const void * b){
  if (*(double*)a > *(double*)b)
    return 1;
  else if (*(double*)a < *(double*)b)
    return -1;
  else
    return 0;  
}

void task2(void* parameter) {

  int i;
  while (true) {
    vTaskDelay(100/portTICK_PERIOD_MS);
        
    if(flag==1){

      /*FFT*/
      FFT.DCRemoval();
      FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.Compute(FFT_FORWARD);
      FFT.ComplexToMagnitude();

    

    /*
     //Extracción de octavas
      octave[0]=vReal[2]/1.5;         // 125Hz  2
      octave[1]=vReal[4]/1.2;         // 250Hz  4
      octave[2]=vReal[10];            // 500Hz  10
      octave[3]=vReal[16];            // 1000Hz 16
      octave[4]=vReal[30];            // 2000Hz 30
      octave[5]=vReal[60];            // 4000Hz 60
      octave[6]=vReal[120];           // 8000Hz 120
      octave[7]=vReal[220]/1.3;       // 16000Hz 310

      //LA10yLA90
      if(0.25*(sum/SAMPLES)<(maximo-minimo)){
        LA10 = maximo-0.1*(sum/SAMPLES);
        LA90 = minimo+0.1*(sum/SAMPLES);
      }
      else{
        LA10 = maximo;
        LA90 = minimo;
      }
      LAeq=(sum/SAMPLES);
      
    */

    //Extracción de octavas
    for (int i = 2; i < (SAMPLES/2); i++){
      if (vReal[i] > 2000) { 
        if (i<=2 )             octave[0] = max(octave[0], (int)(vReal[i])); // 125Hz
        if (i >3   && i<=5 )   octave[1] = max(octave[1], (int)(vReal[i])); // 250Hz
        if (i >5   && i<=7 )   octave[2] = max(octave[2], (int)(vReal[i])); // 500Hz
        if (i >7   && i<=15 )  octave[3] = max(octave[3], (int)(vReal[i])); // 1000Hz
        if (i >15  && i<=30 )  octave[4] = max(octave[4], (int)(vReal[i])); // 2000Hz
        if (i >30  && i<=53 )  octave[5] = max(octave[5], (int)(vReal[i])); // 4000Hz
        if (i >53  && i<=200 ) octave[6] = max(octave[6], (int)(vReal[i])); // 8000Hz
        if (i >200           ) octaveds[7] = max(octave[7], (int)(vReal[i])); // 16000Hz
    }


      /*LA10 LA90*/
      LA90=Leq_arr_sort[51];
      LA10=Leq_arr_sort[461];

                 
      flag=0;  //Termina task hasta nueva llamada
      flag2=1; //Señala al envío de datos
    }
    
  }
  vTaskDelete(NULL);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// MAIN /////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#define I2S_TASK_PRI   1
#define I2S_TASK_STACK 2048
#define CALC_TASK_PRI  1
#define DB_TASK_PRI  1

char ntpHora[30];

void setup() {
  setCpuFrequencyMhz(80); // It should run as low as 80MHz
  Serial.begin(115200);
  //initNetwork();
  delay(1000); // Safety
      
  //FreeRTOS task
  samples_queue = xQueueCreate(8, sizeof(sum_queue_t));
  
  sum_queue_t q;
  uint32_t Leq_samples = 0;
  double Leq_sum_sqr = 0;  
  uint32_t rec=0;
  unsigned long time_start;
  unsigned long time_start2;
  unsigned long time_now;
  int i;

  
  time_start=millis();
  
  //Task 1
  xTaskCreate(mic_i2s_reader_task, "Mic I2S Reader", I2S_TASK_STACK/2, NULL, I2S_TASK_PRI, NULL);
  xTaskCreate(task2 , "Calculo", I2S_TASK_STACK/2, NULL, CALC_TASK_PRI, NULL);
  //xTaskCreate(task3 , "Db", I2S_TASK_STACK/4, NULL, DB_TASK_PRI, NULL);

  while (xQueueReceive(samples_queue, &q, portMAX_DELAY)) {

  
    // dB a MIC_REF_AMPL y ajuste
    double short_RMS = sqrt(double(q.sum_sqr_SPL) / SAMPLES_SHORT);
    double short_SPL_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(short_RMS / MIC_REF_AMPL);
  
    // Si debajo de noise floor measurement, Leq value --> es infinty 
    if (short_SPL_dB > MIC_OVERLOAD_DB) {
      Leq_sum_sqr = INFINITY;
    } else if (isnan(short_SPL_dB) || (short_SPL_dB < MIC_NOISE_DB)) {
      Leq_sum_sqr = -INFINITY;
    }
  
    // Accumulate Leq sum
    Leq_sum_sqr += q.sum_sqr_weighted;
    Leq_samples += SAMPLES_SHORT;
  
    // Calcula Leq value (suf samples)
    if (Leq_samples >= SAMPLE_RATE * LEQ_PERIOD) {
      double Leq_RMS = sqrt(Leq_sum_sqr / Leq_samples);
  
      //MIC_REF_AMPL = pow(10, double(MIC_SENSITIVITY)/20) * ((1<<(MIC_BITS-1))-1);
      //Calculo de los decibelios: 3dB (offset - regulacion) + 96dB (referencia de sensibilidad DATASHEET) + 20 * log10(Leq_RMS / MIC_REF_AMPL)
      //Serial.printf("%.1f // %.1f // %.1f// %.1f\n",MIC_OFFSET_DB,MIC_REF_DB, Leq_RMS,MIC_REF_AMPL);
      
      Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(Leq_RMS / MIC_REF_AMPL);
      Leq_sum_sqr = 0;
      Leq_samples = 0;
      
      // Debug only
      //Serial.printf("%u processing ticks\n", q.proc_ticks);
      
      // Imprimir los valores de Leq_db (tiempo de muestreo 1seg)
      
      Serial.print(30); // To freeze the lower limit
      Serial.print(" ");
      Serial.print(90); // To freeze the upper limit
      Serial.print(" ");
      Serial.printf("%.1f\n", Leq_dB);


      //Preparar muestras
      if (rec>=SAMPLES){
        //Serial.printf("sum: %.1f\n", sum);

      //Serial.printf("LAeq: %.1f\n", LAeq);
      //Serial.printf("max(LA10): %.1f\n", LA10);
      //Serial.printf("min(LA90): %.1f\n", LA90);
      //Serial.printf("octavas: %.1f // %.1f // %.1f // %.1f // %.1f // %.1f // %.1f // %.1f \n", octave[0],octave[1],octave[2],octave[3],octave[4],octave[5],octave[6],octave[7]);
      //Serial.println("listo");

        for (i = 0; i < SAMPLES; i++) {     
          Leq_arr_sort[i] = Leq_arr[i];     
        }
        
        qsort(Leq_arr_sort, SAMPLES, sizeof(double), cmpfunc);
        
        rec=0;
        sum=0;
        maximo=0;
        minimo=100;
        for (i = 0; i < (SAMPLES); i++){
          sum= sum + Leq_arr[i];
          vReal[i] =  Leq_arr[i];

          if(Leq_arr[i] > maximo)maximo = Leq_arr[i];
          if(Leq_arr[i] < minimo)minimo = Leq_arr[i];
        }

//        getHoraNTP(ntpHora);
//        Serial.println(ntpHora);
//        flag=1;
      }

      if (flag2==1){ //Una vez terminado la identificación de Octave, LA10, LA90 y LAeq --> enviar

        //Enviando datos a DB
        network->firestoreDataUpdate(1,Leq_dB,LAeq,LA10,LA90,octave,ntpHora);
        Serial.print("Firestore Database UPDATED  - Parámetros // ");
        Serial.println(ntpHora);
        flag2=0;    
      }

      
      Leq_arr[rec]=Leq_dB;
      rec++;
      

      /*Pasado el tiempo establecido (10m)*/
      time_now=millis();
      if(((time_now-time_start)>SEND_TIME)){
        time_start=time_now;
        getHoraNTP(ntpHora);
        flag=1;
      }else if((time_now-time_start2)>(SEND_TIME/2)){  //SEND_TIME/60 c 1min
        time_start2=time_now;
        
        getHoraNTP(ntpHora);
        network->firestoreDataUpdate(0,Leq_dB,LAeq,LA10,LA90,octave,ntpHora);
        Serial.print("Firestore Database UPDATED  - SPL // ");
        Serial.println(ntpHora);
      }
    }
  }
}

void loop() {
  // Nothing here..
}
