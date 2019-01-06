# CloudVMRealtimeTester
A tool to gather statistics and quantify the reactiveness of Virtual Machines, reporting if they may be trusted to execute hard, firm or soft Real-Time applications

# Audience
DevOps may find this tool usefull to measure if their cloud hosting service do or do not provide the real-time requirements needed by their applications.

# The Problem
Many cloud services SLA's do not mention about the real-time requirements they are able to fulfill. Depending on the virtualization technology, number of users and load those users impose on the shared environment, the quantification may vary -- thus the need for a tool to measure.

# The Solution
This is designed to be left running for a couple of weeks, or even permanently, on each VM instance whose "realtimeness" you want to measure. It works by constantly looping through a timer, which simply measures the time each iteraction took to complete. On a real-time-ready machine, you will always get the same approximate number.

