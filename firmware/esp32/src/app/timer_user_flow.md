```mermaid
graph TD
    IDLE[IDLE State] -->|Single Click| RUNNING[Start Timer]
    IDLE -->|Double Click| EDIT[Enter Editing]
    IDLE -->|Rotate| NAV[Navigate Screens]
    
    RUNNING[RUNNING State] -->|Single Click| PAUSED[Enter Paused]
    RUNNING -->|Double Click| IDLE[Pause + Reset]
    RUNNING -->|Long Press| IDLE[Reset to IDLE]
    RUNNING -->|Rotate| NAV
    
    PAUSED[PAUSED State] -->|Single Click| RUNNING[Resume]
    PAUSED -->|Double Click| IDLE[Reset to IDLE]
    PAUSED -->|Long Press| IDLE[Reset to IDLE]
    PAUSED -->|Rotate| NAV[Navigate Screens]
    
    EDIT[EDITING State] -->|Single Click| NEXT[Next Field (H→M→S)]
    EDIT -->|Double Click| IDLE[Save & Exit]
    EDIT -->|Long Press| IDLE[Cancel & Exit]
    EDIT -->|Rotate| ADJUST_FIELD[Adjust Current Field]
    
    COMPLETE[COMPLETE State] -->|Single Click| EDIT[Enter Editing]
    COMPLETE -->|Double Click| IDLE[Reset + Home]
    COMPLETE -->|Rotate| NAV
```