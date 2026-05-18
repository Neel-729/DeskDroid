#include "date_utils.h"

namespace DateUtils {

int daysInMonth(int year, int month){
  if(month==1 || month==3 || month==5 || month==7 || month==8 || month==10 || month==12) return 31;
  if(month==4 || month==6 || month==9 || month==11) return 30;

  bool leap = (year%4==0 && (year%100!=0 || year%400==0));
  return leap ? 29 : 28;
}

}
