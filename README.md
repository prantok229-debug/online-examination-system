# online-examination-system
This project is a console-based Online Exam System written in C. It supports Teacher and Student roles, persistent storage using text files, time-based exam availability, and dynamic exam/question management.

The system is designed for academic/demo purposes and demonstrates file handling, data persistence, structured programming, and role-based workflows in C.
**PROJECT STRUCTURE**
├── online_exam_final_persistent_fixed.c     # Main source code
├── questions.txt                            # Persistent question bank (auto-created)
├── exams.txt                                # Persistent exam data (auto-created)
├── README.md                                # Project documentation

**Compile**
gcc online_exam_final_persistent_fixed.c -o exam_system
**RUN**
Compile
**Login Credentials (Demo)**
**Role**   **ID to Use**
Student	     STUDENT-ID
Teacher	     TEACHER-ID

Username and numeric password can be any value (demo purpose)

**Core Concepts**

Persistent Storage using text files (questions.txt, exams.txt)

Role-Based Access (Teacher / Student)

Time-Based Exams (start time & end time)

Dynamic Question Bank

Exam Creation, Merging & Updating

Randomized Question Order during Exam
**Program Flow Overview**
Program starts

Demo data is created (if files don’t exist)

User logs in

Role-specific menu is shown

**Teacher Menu**
1) Create or Merge Exam (then add questions)
2) Add Question (save permanently)
3) View Question Bank
4) View Exams
0) Logout
   

**Create or Merge Exam**


-Enter exam title

Enter:

Start time

End time

Duration

-System checks:

If an exam with the same title exists → merge questions

Otherwise → create a new exam

Teacher selects existing questions (by QID)

Teacher can then add new questions

New questions are:

Saved permanently to questions.txt

Automatically appended to the selected exam(s)

Exams file is rewritten to ensure consistency

✅ Students immediately see updated exams.

2️⃣ Add Question

Teacher enters:

Question text

4 options

Correct option (1–4)

Question is:

Assigned a unique QID

Saved to questions.txt

3️⃣ View Question Bank

Displays all stored questions with options and correct answer

4️⃣ View Exams
Displays all exams with:

Exam ID

Title

Time window

Duration

Number of questions



All actions update both memory and files

**Student Workflow (Walkthrough)**
Student Menu
1) Take Exam
2) View Exam Schedule
0) Logout
1️⃣ Take Exam

Step-by-step:

Student enters current time (integer)

System shows available exams based on:

startTime ≤ currentTime ≤ endTime

Student selects an Exam ID

System:

Reloads latest data from files

Collects valid questions

Randomizes question order

Student answers questions (1–4)

Final score is displayed

2️⃣ View Exam Schedule

Displays all exams regardless of time


**File Format Details**
**questions.txt**
[1] What is 2 + 2?
1) 3      2) 4
3) 5      4) 22
[2]
**exams.txt**
[1] Mid Term Exam
[1] [10] [60] [4]
[1,2,3,4]

**Technical Highlights**
Modular design with clear separation of concerns

Safe file rewriting (no duplication bugs)

In-memory + file synchronization

Defensive input handling

Scalable limits (MAXQ, MAXEXAMS)

**Intended Use**

Academic projects

C programming practice

File handling demonstrations

Console application design

 **Notes**

Time is simulated using integers

No real authentication (demo only)

Designed for clarity and learning
