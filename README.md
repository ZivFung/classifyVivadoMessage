# classifyVivadoMessage
This tool could classify the different type of message (warning, critical warning, error) of Vivado project into different files.  

# Usage
./classifyMessage -p [Project_Directory] -s [Synthesis Job Number] -i [Implementation Job Number]  
Output files will generated at current directory.  
Includes "warning.log", "criticalWarning.log" and "error.log".
