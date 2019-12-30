# classifyVivadoMessage
This tool could classify the different type of message (warning, critical warning, error) of Vivado project into different files. This tool can also separate the message that contains file name by file name and give statistic information.  
Ths latest update adds classification of out-of-context message function.
 

# Usage
./classifyMessage -p [Project_Directory] -s [Synthesis Job Number] -i [Implementation Job Number]  
Output files will generated at current directory.  
Includes "warning.log", "criticalWarning.log", "error.log" and "oc_message.log".  
