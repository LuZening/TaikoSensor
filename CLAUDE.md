# CLAUDE.md - Project Guidelines for Claude Code

## Important: CubeMX Code Generation

When editing STM32CubeMX generated files (especially main.c), follow these rules:

### USER CODE Sections
All custom code MUST be placed strictly between `/* USER CODE BEGIN xxx */` and `/* USER CODE END xxx */` labels. This ensures:

1. CubeMX can regenerate peripheral initialization code without overwriting custom logic
2. Custom code survives re-generation of the project

### Correct Pattern
```c
/* USER CODE BEGIN Includes */
#include "my_custom_header.h"
/* USER CODE END Includes */
```

### Wrong Pattern (will be lost on regeneration)
```c
#include "my_custom_header.h"  // Outside USER CODE section - WRONG

/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

// Function defined here - WRONG, outside USER CODE section
void my_function(void) { }
```

### USER CODE Section Locations in main.c
- `USER CODE BEGIN Includes` - Add includes
- `USER CODE BEGIN PV` - Add private variables
- `USER CODE BEGIN 0` - Add forward declarations and function prototypes
- `USER CODE BEGIN 2` - Add initialization code in main()
- `USER CODE BEGIN WHILE` - Add main loop code
- `USER CODE BEGIN 3` - Code at end of main loop
- `USER CODE BEGIN 4` - Add callback functions (ADC, timer, etc.)

## Project-Specific Guidelines

- This is an STM32F103C8T6 piezosensor drum controller project
- Target: 48kHz ADC sampling with FSM-based trigger detection
- Output: USB HID keyboard or drum controller mode