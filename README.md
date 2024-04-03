# Concurrent Student Management System Simulation
This program simulates a concurrent student management system where students seek assistance from tutors. The system consists of multiple student threads, tutor threads, and a coordinator thread to manage the flow of students and tutors.

# Overview
- The simulation involves the following components:


- ** Students **: Represented as threads, students seek help from tutors by occupying available chairs in a waiting area. Each student has a maximum limit on how many help sessions they can receive.

- ** Tutors **: Also represented as threads, tutors assist students who occupy chairs in the waiting area. Tutors conduct tutoring sessions with students and keep track of the total sessions conducted.

- ** Coordinator **: Responsible for coordinating the arrival of students and their insertion into the waiting buffer. The coordinator interacts with student and tutor threads to manage the flow of students seeking help.

# compilation 
```
gcc csmc.c –o csmc -Wall -Werror -pthread -std=gnu99
```
# Usage 
```
./csmc #<students #tutors #chairs #help 

```
# Expected output 
- program must output the following at appropriate times.

- Output of a student thread (x and y are ids):
  - S: Student x takes a seat. Empty chairs = <# of empty chairs after student x took a seat>.
  - S: Student x found no empty chair. Will try again later.
  - S: Student x received help from Tutor y.
- Output of the coordinator threads (x is the id, and p is the priority):
  - C: Student x with priority p added to the queue. Waiting students now = <# students waiting>. Total requests = <total # requests (notifications sent) by students for tutoring so far>
- Output of a tutor thread after tutoring a student (x and y are ids):
  - Student x tutored by Tutor y. Students tutored now = <# students receiving help now>. Total sessions tutored = <total number of tutoring sessions completed so far by all the tutors>

# Contributors
- PJ Avinash 
- Sandeep Reddy Daida

# Note
- tested well on x86 servers
- Might cause issues on ARM machines due to its memory model ( need to use memory barriers for that) 
