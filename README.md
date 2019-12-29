# classifyVivadoMessage
This tool could classify the different type of message (warning, critical warning, error) of Vivado project into different files.
The latest update can classify the info with file name to the output file respectively. 

# Usage
./classifyMessage -p [Project_Directory] -s [Synthesis Job Number] -i [Implementation Job Number]  
Output files will generated at current directory.  
Includes "warning.log", "criticalWarning.log" and "error.log".
