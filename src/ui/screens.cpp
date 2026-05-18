#include "screens.h"

#include <string.h>

#include "../drivers/lcd_driver.h"
#include "../features/reminders.h"
#include "../features/stopwatch.h"
#include "../features/timer.h"

namespace {

void renderLightScheduleEdit(const char* title, uint8_t hour, uint8_t minute, bool blinkState, bool hourSelected){
  LcdDriver::writeRow(0,title);

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
  LcdDriver::writeRow(1,buf);
}

}

namespace UiScreens {

void renderBootTitle(const char* firmwareVersion){
  char buf[17];
  snprintf(buf,sizeof(buf),"DeskDroid v%s",firmwareVersion);
  LcdDriver::writeRow(0,buf);
}

void renderBootScreen(const char* firmwareVersion, const char* status){
  renderBootTitle(firmwareVersion);
  LcdDriver::writeRow(1,status);
}

void renderRtcErrorScreen(){
  LcdDriver::writeRow(0,"RTC ERROR");
}

void renderClockScreen(const char* timeRow, const char* quoteRow){
  LcdDriver::writeRow(0,timeRow);
  LcdDriver::writeRow(1,quoteRow);
}

void renderSettingsScreen(AppState state, const DeviceSettings &settings, const SettingsFlow::Snapshot &renderState){
  if(state==STATE_SETTINGS_HOME){
    LcdDriver::writeRow(0,"Settings");
    LcdDriver::writeRow(1,"Enter");
    return;
  }

  if(state==STATE_SETTINGS_MENU){
    LcdDriver::writeRow(0,"Settings");
    char buf[17];
    snprintf(buf,sizeof(buf),"> %s",renderState.selectedLabel);
    LcdDriver::writeRow(1,buf);
    return;
  }

  if(state!=STATE_SETTINGS_EDIT) return;

  switch(renderState.selectedIndex){
    case 0:
      LcdDriver::writeRow(0,"Backlight");
      LcdDriver::writeRow(1,settings.brightness?"ON":"OFF");
      break;

    case 1:{
      const char* modes[] = {"Off","Static","Breath","Rainbow","Pulse"};
      LcdDriver::writeRow(0,"LED Mode");
      LcdDriver::writeRow(1,modes[settings.idlePreset]);
      break;
    }

    case 2:{
      char buf[17];
      LcdDriver::writeRow(0,"LED Brightness");
      snprintf(buf,sizeof(buf),"Level: %d",settings.ledBrightness);
      LcdDriver::writeRow(1,buf);
      break;
    }

    case 3:
      LcdDriver::writeRow(0,"Auto Lights");
      LcdDriver::writeRow(1,settings.autoLights?"ON":"OFF");
      break;

    case 4:
      renderLightScheduleEdit("Lights On",settings.lightsOnHour,settings.lightsOnMinute,renderState.blinkState,renderState.scheduleHourSelected);
      break;

    case 5:
      renderLightScheduleEdit("Lights Off",settings.lightsOffHour,settings.lightsOffMinute,renderState.blinkState,renderState.scheduleHourSelected);
      break;

    case 6:
      LcdDriver::writeRow(0,"Buzzer");
      LcdDriver::writeRow(1,settings.buzzer?"ON":"OFF");
      break;

    case 7:
      LcdDriver::writeRow(0,"Quotes");
      LcdDriver::writeRow(1,settings.quotes?"ON":"OFF");
      break;

    case 8:
      LcdDriver::writeRow(0,"Time Format");
      LcdDriver::writeRow(1,settings.format24?"24H":"12H");
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
      LcdDriver::writeRow(0,"Adjust Time");
      snprintf(buf,sizeof(buf),"%s:%s",hbuf,mbuf);
      LcdDriver::writeRow(1,buf);
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
      LcdDriver::writeRow(0,"Adjust Date");
      snprintf(buf,sizeof(buf),"%s/%s/%s",dbuf,mbuf,ybuf);
      LcdDriver::writeRow(1,buf);
      break;
    }

    case 11:
      LcdDriver::writeRow(0,"DeskDroid");
      char versionBuf[17];
      snprintf(versionBuf,sizeof(versionBuf),"v%s",renderState.firmwareVersion);
      LcdDriver::writeRow(1,versionBuf);
      break;
  }
}

void renderTimerScreen(AppState state, unsigned long now, bool blinkState){
  if(state==STATE_TIMER_ALARM){
    unsigned long total=TimerFeature::totalMillis();
    int h=total/3600000;
    int m=(total/60000)%60;
    int s=(total/1000)%60;
    char buf[17];
    LcdDriver::writeRow(0,"Timer Finished!");
    snprintf(buf,sizeof(buf),"%02d:%02d:%02d",h,m,s);
    LcdDriver::writeRow(1,buf);
    return;
  }

  if(state==STATE_TIMER_EDIT){
    int h=TimerFeature::hours();
    int m=TimerFeature::minutes();
    int s=TimerFeature::seconds();
    if(!blinkState){
      if(TimerFeature::editField()==EDIT_HOURS) h=-1;
      if(TimerFeature::editField()==EDIT_MINUTES) m=-1;
      if(TimerFeature::editField()==EDIT_SECONDS) s=-1;
    }

    char hbuf[3];
    char mbuf[3];
    char sbuf[3];
    if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,sizeof(hbuf),"%02d",h);
    if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,sizeof(mbuf),"%02d",m);
    if(s<0) strcpy(sbuf,"  "); else snprintf(sbuf,sizeof(sbuf),"%02d",s);

    char buf[17];
    LcdDriver::writeRow(0,"Timer > Edit");
    snprintf(buf,sizeof(buf),"%s:%s:%s",hbuf,mbuf,sbuf);
    LcdDriver::writeRow(1,buf);
    return;
  }

  if(TimerFeature::isRunning()){
    if(blinkState) LcdDriver::writeRow(0,"Timer Running");
    else LcdDriver::writeRow(0,"Timer        ");
  } else {
    if(TimerFeature::remainingMillis(now)==TimerFeature::totalMillis()) LcdDriver::writeRow(0,"Timer");
    else LcdDriver::writeRow(0,"Timer Paused");
  }

  unsigned long remaining = TimerFeature::remainingMillis(now);
  int h=remaining/3600000;
  int m=(remaining/60000)%60;
  int s=(remaining/1000)%60;
  char buf[17];
  snprintf(buf,sizeof(buf),"%02d:%02d:%02d",h,m,s);
  LcdDriver::writeRow(1,buf);
}

void renderStopwatchScreen(){
  unsigned long t=StopwatchFeature::elapsed();
  int minutes=(t/60000)%60;
  int seconds=(t/1000)%60;
  int ms=(t%1000)/10;
  char buf[17];
  LcdDriver::writeRow(0,"Stopwatch");
  snprintf(buf,sizeof(buf),"%02d:%02d.%02d",minutes,seconds,ms);
  LcdDriver::writeRow(1,buf);
}

void renderRemindersScreen(AppState state, bool format24, bool blinkState){
  if(state == STATE_REMINDER_HOME){
    uint8_t h,m;
    LcdDriver::writeRow(0,"Reminders");

    if(RemindersFeature::getNext(h,m)){
      char buf[17];
      int displayHour = h;
      if(!format24){
        if(displayHour == 0) displayHour = 12;
        else if(displayHour > 12) displayHour -= 12;
      }
      snprintf(buf,sizeof(buf),"Next at %02d:%02d",displayHour,m);
      LcdDriver::writeRow(1,buf);
    } else {
      LcdDriver::writeRow(1,"No reminders");
    }
    return;
  }

  if(state == STATE_REMINDER_LIST){
    char top[17];
    snprintf(top,sizeof(top),"Rem %d/%d %s", RemindersFeature::selectedIndex()+1, RemindersFeature::MAX_REMINDERS, RemindersFeature::selectedActive() ? "ON ":"OFF");
    LcdDriver::writeRow(0,top);
    if(!RemindersFeature::selectedActive()){
      LcdDriver::writeRow(1,"Empty");
      return;
    }

    char buf[17];
    snprintf(buf,sizeof(buf),"%02d:%02d", RemindersFeature::selectedHour(), RemindersFeature::selectedMinute());
    LcdDriver::writeRow(1,buf);
    return;
  }

  if(state == STATE_REMINDER_EDIT){
    int rawHour = RemindersFeature::selectedHour();
    int h = rawHour;
    int m = RemindersFeature::selectedMinute();

    if(!format24){
      if(h == 0) h = 12;
      else if(h > 12) h -= 12;
    }
    if(!blinkState){
      if(RemindersFeature::editField()==REM_EDIT_HOUR) h=-1;
      else m=-1;
    }

    char hbuf[3];
    char mbuf[3];
    if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,sizeof(hbuf),"%02d",h);
    if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,sizeof(mbuf),"%02d",m);

    char buf[17];
    LcdDriver::writeRow(0,"Edit Reminder");
    snprintf(buf,sizeof(buf),"%s:%s %s", hbuf, mbuf, RemindersFeature::selectedActive() ? "ON":"OFF");
    LcdDriver::writeRow(1,buf);
  }
}

void renderReminderAlarmScreen(){
  char buf0[17];
  char buf1[17];
  snprintf(buf0,sizeof(buf0),"Reminder %d",RemindersFeature::activeAlarmIndex()+1);
  snprintf(buf1,sizeof(buf1),"%02d:%02d",RemindersFeature::activeAlarmHour(),RemindersFeature::activeAlarmMinute());
  LcdDriver::writeRow(0,buf0);
  LcdDriver::writeRow(1,buf1);
}

}
