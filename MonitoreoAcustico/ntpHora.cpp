#include "ntpHora.h"

#include <WiFi.h>
#include "time.h"

int second;
int minute;
int hour;
int day;
int month;
int year;

struct tm timeinfo;

  /*
struct tm
{
int    tm_sec;   //   Seconds [0,60]. 
int    tm_min;   //   Minutes [0,59]. 
int    tm_hour;  //   Hour [0,23]. 
int    tm_mday;  //   Day of month [1,31]. 
int    tm_mon;   //   Month of year [0,11]. 
int    tm_year;  //   Years since 1900. 
int    tm_wday;  //   Day of week [0,6] (Sunday =0). 
int    tm_yday;  //   Day of year [0,365]. 
int    tm_isdst; //   Daylight Savings flag. 
}
 */ 

char second_str[10];
char minute_str[10];
char hour_str[10];
char day_str[10];
char month_str[10];
char year_str[10];

void printLocalTime()
{

  if(!getLocalTime(&timeinfo)){
    Serial.println("Set-up NTP inicial");
    return;
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void getHoraNTP(char* ntpHora){

  char HoraNTP[30]={};

  printLocalTime();
  second = timeinfo.tm_sec;
  minute = timeinfo.tm_min;
  hour = timeinfo.tm_hour;
  day = timeinfo.tm_mday;
  month = timeinfo.tm_mon + 1;
  year = timeinfo.tm_year + 1900;


  itoa(second,second_str,10);
  itoa(minute,minute_str,10);
  itoa(hour,hour_str,10);
  itoa(day,day_str,10);
  itoa(month,month_str,10);
  itoa(year,year_str,10);

  //"DD-MM-YY HH:MM:SS"
  strcat(HoraNTP,"_");
  if(day<10){
    strcat(HoraNTP,"0");
  }
  strcat(HoraNTP,day_str);
  strcat(HoraNTP,"-");
  if(month<10){
    strcat(HoraNTP,"0");
  }  
  strcat(HoraNTP,month_str);
  strcat(HoraNTP,"-");
  strcat(HoraNTP,year_str);
  strcat(HoraNTP,"___");
  if(hour<10){
    strcat(HoraNTP,"0");
  }  
  strcat(HoraNTP,hour_str);
  strcat(HoraNTP,":");
  if(minute<10){
    strcat(HoraNTP,"0");
  }  
  strcat(HoraNTP,minute_str);
  strcat(HoraNTP,":");
  if(second<10){
    strcat(HoraNTP,"0");
  }  
  strcat(HoraNTP,second_str);
  strcat(HoraNTP,"_");

  strcpy(ntpHora,HoraNTP);
}
