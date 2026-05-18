#include "screens.h"

#include <string.h>

namespace {

char frameRows[2][17] = {
  "                ",
  "                "
};

void writeRow(uint8_t row, const char* text){
  if(row >= 2) return;

  uint8_t i = 0;
  while(i < 16 && text[i] != '\0'){
    frameRows[row][i] = text[i];
    i++;
  }
  while(i < 16){
    frameRows[row][i] = ' ';
    i++;
  }
  frameRows[row][16] = '\0';
}

void renderLightScheduleEdit(const char* title, uint8_t hour, uint8_t minute, bool blinkState, bool hourSelected){
  writeRow(0,title);

  int h=hour;
  int m=minute;
  if(!blinkState){
    if(hourSelected) h=-1;
    else m=-1;
  }

  char hbuf[3];
  char mbuf[3];
  if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,sizeof(hbuf),"%02d",h);
  if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,sizeof(mbuf),"%02d",m);

  char buf[17];
  snprintf(buf,sizeof(buf),"%s:%s",hbuf,mbuf);
  writeRow(1,buf);
}

}

namespace UiScreens {

const char* row(uint8_t index){
  if(index >= 2) return "";
  return frameRows[index];
}

void clearFrame(){
  writeRow(0, "");
  writeRow(1, "");
}

void renderBootTitle(const char* firmwareVersion){
  char buf[17];
  snprintf(buf,sizeof(buf),"DeskDroid v%s",firmwareVersion);
  writeRow(0,buf);
}

void renderBootScreen(const char* firmwareVersion, const char* status){
  renderBootTitle(firmwareVersion);
  writeRow(1,status);
}

void renderRtcErrorScreen(){
  writeRow(0,"RTC ERROR");
}

void renderClockScreen(const char* timeRow, const char* quoteRow){
  writeRow(0,timeRow);
  writeRow(1,quoteRow);
}

void renderSettingsScreen(AppState state, const DeviceSettings &settings, const SettingsFlow::Snapshot &renderState){
  if(state==STATE_SETTINGS_HOME){
    writeRow(0,"Settings");
    writeRow(1,"Enter");
    return;
  }

  if(state==STATE_SETTINGS_MENU){
    writeRow(0,"Settings");
    char buf[17];
    snprintf(buf,sizeof(buf),"> %s",renderState.selectedLabel);
    writeRow(1,buf);
    return;
  }

  if(state!=STATE_SETTINGS_EDIT) return;

  switch(renderState.selectedIndex){
    case 0:
      writeRow(0,"Backlight");
      writeRow(1,settings.brightness?"ON":"OFF");
      break;

    case 1:{
      const char* modes[] = {"Off","Static","Breath","Rainbow","Pulse"};
      writeRow(0,"LED Mode");
      writeRow(1,modes[settings.idlePreset]);
      break;
    }

    case 2:{
      char buf[17];
      writeRow(0,"LED Brightness");
      snprintf(buf,sizeof(buf),"Level: %d",settings.ledBrightness);
      writeRow(1,buf);
      break;
    }

    case 3:
      writeRow(0,"Auto Lights");
      writeRow(1,settings.autoLights?"ON":"OFF");
      break;

    case 4:
      renderLightScheduleEdit("Lights On",settings.lightsOnHour,settings.lightsOnMinute,renderState.blinkState,renderState.scheduleHourSelected);
      break;

    case 5:
      renderLightScheduleEdit("Lights Off",settings.lightsOffHour,settings.lightsOffMinute,renderState.blinkState,renderState.scheduleHourSelected);
      break;

    case 6:
      writeRow(0,"Buzzer");
      writeRow(1,settings.buzzer?"ON":"OFF");
      break;

    case 7:
      writeRow(0,"Quotes");
      writeRow(1,settings.quotes?"ON":"OFF");
      break;

    case 8:
      writeRow(0,"Time Format");
      writeRow(1,settings.format24?"24H":"12H");
      break;

    case 9:{
      int h=renderState.adjustHour;
      int m=renderState.adjustMinute;
      if(!renderState.blinkState){
        if(renderState.adjustHourSelected) h=-1;
        else m=-1;
      }

      char hbuf[3];
      char mbuf[3];
      if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,sizeof(hbuf),"%02d",h);
      if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,sizeof(mbuf),"%02d",m);

      char buf[17];
      writeRow(0,"Adjust Time");
      snprintf(buf,sizeof(buf),"%s:%s",hbuf,mbuf);
      writeRow(1,buf);
      break;
    }

    case 10:{
      int d=renderState.adjustDay;
      int mo=renderState.adjustMonth;
      int y=renderState.adjustYear;

      if(!renderState.blinkState){
        if(renderState.adjustDateField==0) d=-1;
        else if(renderState.adjustDateField==1) mo=-1;
        else y=-1;
      }

      char dbuf[3];
      char mbuf[3];
      char ybuf[5];
      if(d<0) strcpy(dbuf,"  "); else snprintf(dbuf,sizeof(dbuf),"%02d",d);
      if(mo<0) strcpy(mbuf,"  "); else snprintf(mbuf,sizeof(mbuf),"%02d",mo);
      if(y<0) strcpy(ybuf,"    "); else snprintf(ybuf,sizeof(ybuf),"%04d",y);

      char buf[17];
      writeRow(0,"Adjust Date");
      snprintf(buf,sizeof(buf),"%s/%s/%s",dbuf,mbuf,ybuf);
      writeRow(1,buf);
      break;
    }

    case 11:
      writeRow(0,"DeskDroid");
      char versionBuf[17];
      snprintf(versionBuf,sizeof(versionBuf),"v%s",renderState.firmwareVersion);
      writeRow(1,versionBuf);
      break;
  }
}

void renderTimerScreen(AppState state, const TimerScreenData &data, bool blinkState){
  if(state==STATE_TIMER_ALARM){
    unsigned long total=data.totalMillis;
    int h=total/3600000;
    int m=(total/60000)%60;
    int s=(total/1000)%60;
    char buf[17];
    writeRow(0,"Timer Finished!");
    snprintf(buf,sizeof(buf),"%02d:%02d:%02d",h,m,s);
    writeRow(1,buf);
    return;
  }

  if(state==STATE_TIMER_EDIT){
    int h=data.hours;
    int m=data.minutes;
    int s=data.seconds;
    if(!blinkState){
      if(data.editField==0) h=-1;
      if(data.editField==1) m=-1;
      if(data.editField==2) s=-1;
    }

    char hbuf[3];
    char mbuf[3];
    char sbuf[3];
    if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,sizeof(hbuf),"%02d",h);
    if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,sizeof(mbuf),"%02d",m);
    if(s<0) strcpy(sbuf,"  "); else snprintf(sbuf,sizeof(sbuf),"%02d",s);

    char buf[17];
    writeRow(0,"Timer > Edit");
    snprintf(buf,sizeof(buf),"%s:%s:%s",hbuf,mbuf,sbuf);
    writeRow(1,buf);
    return;
  }

  if(data.running){
    if(blinkState) writeRow(0,"Timer Running");
    else writeRow(0,"Timer        ");
  } else {
    if(data.remainingMillis==data.totalMillis) writeRow(0,"Timer");
    else writeRow(0,"Timer Paused");
  }

  unsigned long remaining = data.remainingMillis;
  int h=remaining/3600000;
  int m=(remaining/60000)%60;
  int s=(remaining/1000)%60;
  char buf[17];
  snprintf(buf,sizeof(buf),"%02d:%02d:%02d",h,m,s);
  writeRow(1,buf);
}

void renderStopwatchScreen(const StopwatchScreenData &data){
  unsigned long t=data.elapsedMillis;
  int minutes=(t/60000)%60;
  int seconds=(t/1000)%60;
  int ms=(t%1000)/10;
  char buf[17];
  writeRow(0,"Stopwatch");
  snprintf(buf,sizeof(buf),"%02d:%02d.%02d",minutes,seconds,ms);
  writeRow(1,buf);
}

void renderRemindersScreen(AppState state, const RemindersScreenData &data, bool format24, bool blinkState){
  if(state == STATE_REMINDER_HOME){
    writeRow(0,"Reminders");

    if(data.hasNext){
      char buf[17];
      int displayHour = data.nextHour;
      if(!format24){
        if(displayHour == 0) displayHour = 12;
        else if(displayHour > 12) displayHour -= 12;
      }
      snprintf(buf,sizeof(buf),"Next at %02d:%02d",displayHour,data.nextMinute);
      writeRow(1,buf);
    } else {
      writeRow(1,"No reminders");
    }
    return;
  }

  if(state == STATE_REMINDER_LIST){
    char top[17];
    snprintf(top,sizeof(top),"Rem %d/%d %s", data.selectedIndex+1, data.maxReminders, data.selectedActive ? "ON ":"OFF");
    writeRow(0,top);
    if(!data.selectedActive){
      writeRow(1,"Empty");
      return;
    }

    char buf[17];
    snprintf(buf,sizeof(buf),"%02d:%02d", data.selectedHour, data.selectedMinute);
    writeRow(1,buf);
    return;
  }

  if(state == STATE_REMINDER_EDIT){
    int h = data.selectedHour;
    int m = data.selectedMinute;

    if(!format24){
      if(h == 0) h = 12;
      else if(h > 12) h -= 12;
    }
    if(!blinkState){
      if(data.editField==0) h=-1;
      else m=-1;
    }

    char hbuf[3];
    char mbuf[3];
    if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,sizeof(hbuf),"%02d",h);
    if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,sizeof(mbuf),"%02d",m);

    char buf[17];
    writeRow(0,"Edit Reminder");
    snprintf(buf,sizeof(buf),"%s:%s %s", hbuf, mbuf, data.selectedActive ? "ON":"OFF");
    writeRow(1,buf);
  }
}

void renderReminderAlarmScreen(const RemindersScreenData &data){
  char buf0[17];
  char buf1[17];
  snprintf(buf0,sizeof(buf0),"Reminder %d",data.activeAlarmIndex+1);
  snprintf(buf1,sizeof(buf1),"%02d:%02d",data.activeAlarmHour,data.activeAlarmMinute);
  writeRow(0,buf0);
  writeRow(1,buf1);
}

}
