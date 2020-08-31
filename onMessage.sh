#!/bin/bash
cd /coding/mailbox
log=protocol.log

rssi=$1
length=$2
weigth=$3
battery=$4

echo $(date)   RSSI: $rssi Länge: $length Gewicht: $weigth   Batterie: $battery>> $log

# ToDo/Outlook:
# - Save data to database
# - create correlation between weigth number and grams unit
# - big data analysis / machine learning on mail content weight. e.g. standard letter weight is app. 10 gram
# - Connect Telegram bot to inform about new mail
