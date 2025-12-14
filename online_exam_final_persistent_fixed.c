/* online_exam_final_persistent_fixed.c1
   Fixed version:
   - Proper rewrite_all_exams_file() implementation (no double-write)
   - Reload questions/exams after teacher changes
   - Reload before student takes exam
   - Maintains previous behavior & formats
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define QUESTIONS_FILE "questions.txt"
#define EXAMS_FILE "exams.txt"
#define MAXQ 1000
#define MAXTEXT 512
#define MAXEXAMS 500
#define LINEBUF 1024

typedef struct {
    int qid;
    char text[MAXTEXT];
    char opt[4][256];
    int correct; // 1..4
} Question;

typedef struct {
    int examId;
    char title[256];
    int startTime;
    int endTime;
    int duration;
    int qCount;
    int qids[200];
} Exam;

/* In-memory storage */
static Question qbank[MAXQ];
static int qcount = 0;
static Exam ebank[MAXEXAMS];
static int ecount = 0;

/* Utility functions */
static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n>0 && (s[n-1]=='\n' || s[n-1]=='\r')) { s[n-1]=0; n--; }
}
static int file_exists(const char *path) {
    FILE *f = fopen(path,"r"); if (!f) return 0; fclose(f); return 1;
}
static int read_line(FILE *f, char *buf, int size) {
    if (!fgets(buf, size, f)) return 0;
    trim_newline(buf);
    return 1;
}

/* Normalize title to compare: remove whitespace and lowercase */
static void normalize_title(const char *src, char *out, int outsize) {
    int j=0;
    for (int i=0; src[i] && j+1<outsize; ++i) {
        if (!isspace((unsigned char)src[i])) out[j++] = tolower((unsigned char)src[i]);
    }
    out[j]=0;
}

/* -------------------- Demo loader -------------------- */
static void load_demo_questions_memory() {
    qcount = 0;
    Question q;
    q.qid = 1;
    strcpy(q.text, "What is 2 + 2?");
    strcpy(q.opt[0],"3"); strcpy(q.opt[1],"4"); strcpy(q.opt[2],"5"); strcpy(q.opt[3],"22"); q.correct = 2; qbank[qcount++]=q;
    q.qid = 2;
    strcpy(q.text, "Capital of France?");
    strcpy(q.opt[0],"London"); strcpy(q.opt[1],"Berlin"); strcpy(q.opt[2],"Paris"); strcpy(q.opt[3],"Madrid"); q.correct = 3; qbank[qcount++]=q;
    q.qid = 3;
    strcpy(q.text, "Who created the C language?");
    strcpy(q.opt[0],"Charles Babbage"); strcpy(q.opt[1],"Dennis Ritchie"); strcpy(q.opt[2],"Bjarne Stroustrup"); strcpy(q.opt[3],"Alan Turing"); q.correct = 2; qbank[qcount++]=q;
    q.qid = 4;
    strcpy(q.text, "Which one is an operating system?");
    strcpy(q.opt[0],"Linux"); strcpy(q.opt[1],"Google"); strcpy(q.opt[2],"Intel"); strcpy(q.opt[3],"Microsoft Office"); q.correct = 1; qbank[qcount++]=q;
}

/* -------------------- File parsing: Questions -------------------- */
static int parse_question_block(FILE *f, char *firstLine) {
    if (!firstLine) return 0;
    int qid=0;
    char *p = firstLine;
    if (*p!='[') return 0;
    p++;
    while (*p && isdigit((unsigned char)*p)) { qid = qid*10 + (*p - '0'); p++; }
    while (*p && *p != ']') p++;
    if (*p==']') p++;
    while (*p && isspace((unsigned char)*p)) p++;
    char qtext[MAXTEXT]; strncpy(qtext, p, MAXTEXT-1); qtext[MAXTEXT-1]=0;

    char line1[LINEBUF], line2[LINEBUF], line3[LINEBUF];
    if (!read_line(f, line1, sizeof(line1))) return 0;
    if (!read_line(f, line2, sizeof(line2))) return 0;
    if (!read_line(f, line3, sizeof(line3))) return 0;

    char *pos1 = strstr(line1, "1)");
    char *pos2 = strstr(line1, "2)");
    char *pos3 = strstr(line2, "3)");
    char *pos4 = strstr(line2, "4)");
    if (!pos1 || !pos2 || !pos3 || !pos4) return 0;
    pos1 += 2; while (*pos1 && isspace((unsigned char)*pos1)) pos1++;
    char opt1[256]; int len = (int)(pos2 - pos1); if (len<=0) return 0; strncpy(opt1,pos1,len); opt1[len]=0; trim_newline(opt1);
    pos2 += 2; while (*pos2 && isspace((unsigned char)*pos2)) pos2++; char opt2[256]; strncpy(opt2,pos2,255); opt2[255]=0; trim_newline(opt2);
    pos3 += 2; while (*pos3 && isspace((unsigned char)*pos3)) pos3++; char opt3[256]; len=(int)(pos4-pos3); if (len<=0) return 0; strncpy(opt3,pos3,len); opt3[len]=0; trim_newline(opt3);
    pos4 +=2; while (*pos4 && isspace((unsigned char)*pos4)) pos4++; char opt4[256]; strncpy(opt4,pos4,255); opt4[255]=0; trim_newline(opt4);

    int correct=1;
    char *c = line3; while (*c && *c!='[') c++; if (*c=='[') c++;
    int val=0; while (*c && isdigit((unsigned char)*c)) { val = val*10 + (*c - '0'); c++; } if (val>=1 && val<=4) correct=val;

    if (qcount < MAXQ) {
        Question qq;
        qq.qid = qid; strncpy(qq.text,qtext,MAXTEXT-1); qq.text[MAXTEXT-1]=0;
        strncpy(qq.opt[0],opt1,255); qq.opt[0][255]=0;
        strncpy(qq.opt[1],opt2,255); qq.opt[1][255]=0;
        strncpy(qq.opt[2],opt3,255); qq.opt[2][255]=0;
        strncpy(qq.opt[3],opt4,255); qq.opt[3][255]=0;
        qq.correct = correct;
        qbank[qcount++] = qq;
        return 1;
    }
    return 0;
}

static void load_questions_from_file() {
    qcount = 0;
    FILE *f = fopen(QUESTIONS_FILE,"r");
    if (!f) return;
    char line[LINEBUF];
    while (read_line(f, line, sizeof(line))) {
        if (line[0]=='[') parse_question_block(f, line);
    }
    fclose(f);
}

/* Save question appended in the C1 aligned style with dynamic spacing */
static void save_question_to_file(const Question *q) {
    FILE *f = fopen(QUESTIONS_FILE,"a");
    if (!f) { printf("Error opening questions file for write.\n"); return; }
    fprintf(f,"[%d] %s\n", q->qid, q->text);
    int l1 = (int)strlen(q->opt[0]), l3 = (int)strlen(q->opt[2]);
    int leftMax = l1>l3?l1:l3;
    int colWidth = 3 + leftMax + 6;
    fprintf(f,"1) %s", q->opt[0]);
    int cur = 3 + (int)strlen(q->opt[0]);
    for (int i=cur;i<colWidth;i++) fputc(' ', f);
    fprintf(f,"2) %s\n", q->opt[1]);
    fprintf(f,"3) %s", q->opt[2]);
    cur = 3 + (int)strlen(q->opt[2]);
    for (int i=cur;i<colWidth;i++) fputc(' ', f);
    fprintf(f,"4) %s\n", q->opt[3]);
    fprintf(f,"[%d]\n\n", q->correct);
    fclose(f);
}

/* return next qid (max existing + 1) */
static int next_qid() {
    int maxid = 0;
    for (int i=0;i<qcount;i++) if (qbank[i].qid>maxid) maxid=qbank[i].qid;
    return maxid+1;
}

/* -------------------- Exams file parse/save -------------------- */
static int parse_exam_block(FILE *f, char *firstLine) {
    if (!firstLine) return 0;
    int examId=0; char *p = firstLine;
    if (*p!='[') return 0; p++;
    while (*p && isdigit((unsigned char)*p)) { examId = examId*10 + (*p - '0'); p++; }
    while (*p && *p!=']') p++; if (*p==']') p++;
    while (*p && isspace((unsigned char)*p)) p++;
    Exam ex; ex.examId = examId; strncpy(ex.title, p, 255); ex.title[255]=0;
    char line2[LINEBUF], line3[LINEBUF];
    if (!read_line(f,line2,sizeof(line2))) return 0;
    if (!read_line(f,line3,sizeof(line3))) return 0;
    ex.startTime = ex.endTime = ex.duration = ex.qCount = 0;
    sscanf(line2, "[%d] [%d] [%d] [%d]", &ex.startTime, &ex.endTime, &ex.duration, &ex.qCount);
    char *s = line3; while (*s && *s!='[') s++; if (*s=='[') s++;
    char numbuf[32]; int ni=0;
    ex.qCount = 0;
    while (*s && *s!=']') {
        if (isdigit((unsigned char)*s)) numbuf[ni++]=*s;
        else if (*s==',' || isspace((unsigned char)*s)) {
            if (ni>0) { numbuf[ni]=0; ex.qids[ex.qCount++]=atoi(numbuf); ni=0; }
        }
        s++;
    }
    if (ni>0) { numbuf[ni]=0; ex.qids[ex.qCount++]=atoi(numbuf); ni=0; }
    if (ecount < MAXEXAMS) { ebank[ecount++] = ex; return 1; }
    return 0;
}

static void load_exams_from_file() {
    ecount = 0;
    FILE *f = fopen(EXAMS_FILE,"r");
    if (!f) return;
    char line[LINEBUF];
    while (read_line(f, line, sizeof(line))) {
        if (line[0]=='[') parse_exam_block(f, line);
    }
    fclose(f);
}

/* Append exam to file */
static void save_exam_to_file(const Exam *ex) {
    FILE *f = fopen(EXAMS_FILE,"a");
    if (!f) { printf("Error writing exams file.\n"); return; }
    fprintf(f, "[%d] %s\n", ex->examId, ex->title);
    fprintf(f, "[%d] [%d] [%d] [%d]\n", ex->startTime, ex->endTime, ex->duration, ex->qCount);
    fprintf(f, "[");
    for (int i=0;i<ex->qCount;i++) {
        fprintf(f, "%d", ex->qids[i]);
        if (i < ex->qCount-1) fprintf(f,",");
    }
    fprintf(f, "]\n\n");
    fclose(f);
}

/* Re-write entire exams file (used when updating QID lists) */
static void rewrite_all_exams_file() {
    FILE *g = fopen(EXAMS_FILE,"w");
    if (!g) { printf("Error rewriting exams file.\n"); return; }
    for (int i=0;i<ecount;i++) {
        Exam *ex = &ebank[i];
        fprintf(g, "[%d] %s\n", ex->examId, ex->title);
        fprintf(g, "[%d] [%d] [%d] [%d]\n", ex->startTime, ex->endTime, ex->duration, ex->qCount);
        fprintf(g, "[");
        for (int j=0;j<ex->qCount;j++) {
            fprintf(g, "%d", ex->qids[j]);
            if (j < ex->qCount-1) fprintf(g,",");
        }
        fprintf(g, "]\n\n");
    }
    fclose(g);
}

/* -------------------- Console printing helpers -------------------- */
static void print_question_bank_console() {
    if (qcount == 0) { printf("No questions in bank.\n"); return; }
    for (int i=0;i<qcount;i++) {
        Question *q = &qbank[i];
        printf("\n[%d] %s\n", q->qid, q->text);
        int l1 = (int)strlen(q->opt[0]), l3 = (int)strlen(q->opt[2]);
        int leftMax = l1 > l3 ? l1 : l3;
        int colWidth = 3 + leftMax + 6;
        printf("1) %s", q->opt[0]);
        int cur = 3 + (int)strlen(q->opt[0]);
        for (int sp=cur; sp<colWidth; sp++) putchar(' ');
        printf("2) %s\n", q->opt[1]);
        printf("3) %s", q->opt[2]);
        cur = 3 + (int)strlen(q->opt[2]);
        for (int sp=cur; sp<colWidth; sp++) putchar(' ');
        printf("4) %s\n", q->opt[3]);
        printf("[Correct: %d]\n", q->correct);
    }
    printf("\n");
}

/* -------------------- Teacher flows -------------------- */
static int teacher_add_question_and_save() {
    if (qcount >= MAXQ) { printf("Question bank is full.\n"); return 0; }
    Question q;
    q.qid = next_qid();
    char buf[MAXTEXT];
    printf("Enter question text (single line):\n");
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    trim_newline(buf);
    strncpy(q.text, buf, MAXTEXT-1); q.text[MAXTEXT-1]=0;
    for (int i=0;i<4;i++) {
        printf("Option %d: ", i+1);
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        trim_newline(buf);
        strncpy(q.opt[i], buf, 255); q.opt[i][255]=0;
    }
    int c;
    printf("Correct option (1-4): ");
    while (scanf("%d", &c) != 1) { printf("Enter 1-4: "); while(getchar()!='\n'); }
    while (getchar()!='\n');
    if (c<1 || c>4) c=1;
    q.correct = c;
    qbank[qcount++] = q;
    save_question_to_file(&q);
    printf("Saved question QID=%d\n", q.qid);
    return q.qid;
}

/* Teacher create exam flow */
static void teacher_create_exam_flow() {
    char title[256], buf[LINEBUF];
    printf("Enter exam title (single line):\n");
    if (!fgets(title, sizeof(title), stdin)) return;
    trim_newline(title);
    int startT, endT, duration;
    printf("Enter start time (integer): "); while (scanf("%d", &startT) != 1) { printf("Enter integer: "); while(getchar()!='\n'); }
    printf("Enter end time (integer): "); while (scanf("%d", &endT) != 1) { printf("Enter integer: "); while(getchar()!='\n'); }
    printf("Enter duration in seconds (integer): "); while (scanf("%d", &duration) != 1) { printf("Enter integer: "); while(getchar()!='\n'); }
    while (getchar()!='\n');

    char normNew[512]; normalize_title(title, normNew, sizeof(normNew));
    int matches[20]; int mcount=0;
    for (int i=0;i<ecount;i++) {
        char normExist[512]; normalize_title(ebank[i].title, normExist, sizeof(normExist));
        if (strcmp(normExist, normNew) == 0) {
            matches[mcount++] = i;
        }
    }

    int selectedQIDs[200]; int selCount=0;
    if (qcount==0) {
        printf("Question bank empty.\n");
    } else {
        print_question_bank_console();
        printf("How many existing questions to include? (0-%d): ", qcount);
        int num;
        while (scanf("%d", &num) != 1) { printf("Enter integer: "); while(getchar()!='\n'); }
        while (getchar()!='\n');
        if (num < 0) num = 0;
        if (num > qcount) num = qcount;
        for (int i=0;i<num;i++) {
            int qid;
            printf("Enter QID #%d: ", i+1);
            while (scanf("%d", &qid) != 1) { printf("Enter QID integer: "); while(getchar()!='\n'); }
            while (getchar()!='\n');
            int ok=0;
            for (int j=0;j<qcount;j++) if (qbank[j].qid==qid) { ok=1; break; }
            if (!ok) { printf("Invalid QID, try again.\n"); i--; continue; }
            selectedQIDs[selCount++] = qid;
        }
    }

    if (mcount > 0) {
        printf("Found %d matching existing exam(s); merging selected QIDs into them.\n", mcount);
        for (int mi=0; mi<mcount; ++mi) {
            int idx = matches[mi];
            for (int k=0;k<selCount;k++) {
                ebank[idx].qids[ebank[idx].qCount++] = selectedQIDs[k];
            }
        }
        // after merging selected QIDs, rewrite exams file so students see them immediately
        rewrite_all_exams_file();
        // reload memory to ensure consistent state
        load_questions_from_file();
        load_exams_from_file();
    } else {
        Exam ex; ex.examId = 1;
        for (int i=0;i<ecount;i++) if (ebank[i].examId >= ex.examId) ex.examId = ebank[i].examId + 1;
        strncpy(ex.title, title, 255); ex.title[255]=0;
        ex.startTime = startT; ex.endTime = endT; ex.duration = duration;
        ex.qCount = 0;
        for (int i=0;i<selCount;i++) ex.qids[ex.qCount++] = selectedQIDs[i];
        ebank[ecount++] = ex;
        save_exam_to_file(&ex);
        matches[0] = ecount - 1; mcount = 1;
        // reload exams so memory is consistent
        load_exams_from_file();
        printf("Created new exam (ExamID %d) and saved.\n", ex.examId);
    }

    // Immediately go to add-question section; every new question saved and appended to ALL matched exams
    char cont = 'y';
    while (cont == 'y' || cont == 'Y') {
        printf("\nAdd a new question (this will be saved permanently):\n");
        int newQID = teacher_add_question_and_save(); // appends to qbank and questions.txt
        if (newQID > 0) {
            // append newQID to all matched exams
            for (int mi=0; mi<mcount; ++mi) {
                int idx = matches[mi];
                if (ebank[idx].qCount < 200) {
                    ebank[idx].qids[ebank[idx].qCount++] = newQID;
                }
            }
            // After modifying exams in memory, rewrite exam file to reflect changes
            rewrite_all_exams_file();
            // reload both banks to ensure in-memory state matches files
            load_questions_from_file();
            load_exams_from_file();
            printf("Appended new QID %d to %d matched exam(s) and saved exams file.\n", newQID, mcount);
        } else {
            printf("Question not saved.\n");
        }
        printf("Add another question? (y/n): ");
        if (scanf(" %c", &cont) != 1) cont = 'n';
        while (getchar()!='\n');
    }

    printf("Finished exam creation/merge flow. Returning to Teacher menu.\n");
}

/* Teacher menu */
static void show_teacher_menu() {
    while (1) {
        printf("\n--- TEACHER MENU ---\n");
        printf("1) Create or Merge Exam (then add questions)\n");
        printf("2) Add Question (save permanently)\n");
        printf("3) View Question Bank\n");
        printf("4) View Exams\n");
        printf("0) Logout\n");
        printf("Choose: ");
        int ch; if (scanf("%d", &ch) != 1) { while (getchar()!='\n'); continue; }
        while (getchar()!='\n');
        if (ch==0) { printf("Teacher logged out.\n"); break; }
        else if (ch==1) teacher_create_exam_flow();
        else if (ch==2) {
            teacher_add_question_and_save();
            // ensure memory & files consistent (question file updated by function)
            load_questions_from_file();
        }
        else if (ch==3) print_question_bank_console();
        else if (ch==4) {
            if (ecount==0) printf("No exams saved.\n");
            else {
                printf("\n--- Exams ---\n");
                for (int i=0;i<ecount;i++) {
                    Exam *ex = &ebank[i];
                    printf("ExamID %d : %s | start=%d end=%d duration=%d qCount=%d\n",
                           ex->examId, ex->title, ex->startTime, ex->endTime, ex->duration, ex->qCount);
                }
            }
        }
        else printf("Invalid choice.\n");
    }
}

/* -------------------- Student flows -------------------- */
static void show_schedule_console() {
    if (ecount==0) { printf("No exams available.\n"); return; }
    printf("\n--- Exam Schedule ---\n");
    for (int i=0;i<ecount;i++) {
        Exam *ex = &ebank[i];
        printf("ExamID %d : %s | start=%d end=%d duration=%d qCount=%d\n",
               ex->examId, ex->title, ex->startTime, ex->endTime, ex->duration, ex->qCount);
    }
}

/* Student take exam flow (time-based) - RELOAD before showing exams */
static void student_take_exam_flow(const char *username) {
    // reload latest data so student will see exactly what is saved
    load_questions_from_file();
    load_exams_from_file();

    if (ecount==0) { printf("No exams present.\n"); return; }
    int now;
    printf("Enter current time (integer): ");
    while (scanf("%d", &now) != 1) { printf("Enter integer: "); while(getchar()!='\n'); }
    while (getchar()!='\n');
    int avail = 0;
    printf("\n--- Available exams at time %d ---\n", now);
    for (int i=0;i<ecount;i++) if (now >= ebank[i].startTime && now <= ebank[i].endTime) {
        printf("ExamID %d : %s | duration=%d | qCount=%d\n", ebank[i].examId, ebank[i].title, ebank[i].duration, ebank[i].qCount);
        avail++;
    }
    if (avail==0) { printf("No exams available now.\n"); return; }
    int exid;
    printf("Enter ExamID to start: ");
    while (scanf("%d", &exid) != 1) { printf("Enter valid ExamID: "); while(getchar()!='\n'); }
    while (getchar()!='\n');
    int idx = -1;
    for (int i=0;i<ecount;i++) if (ebank[i].examId == exid) { idx = i; break; }
    if (idx==-1) { printf("Invalid ExamID.\n"); return; }
    Exam *ex = &ebank[idx];
    if (!(now >= ex->startTime && now <= ex->endTime)) { printf("Exam not active now.\n"); return; }
    if (ex->qCount == 0) { printf("Exam has no questions.\n"); return; }

    /* build question list */
    Question examQs[200];
    int qc = 0;
    for (int i=0;i<ex->qCount;i++) {
        int qid = ex->qids[i];
        int found = 0;
        for (int j=0;j<qcount;j++) if (qbank[j].qid == qid) { examQs[qc++] = qbank[j]; found = 1; break; }
        if (!found) printf("Warning: QID %d not found (skipped)\n", qid);
    }
    if (qc==0) { printf("No valid questions in exam.\n"); return; }

    /* randomize order */
    int order[qc];
    for (int i=0;i<qc;i++) order[i]=i;
    for (int i=qc-1;i>0;i--) { int r = rand() % (i+1); int t=order[i]; order[i]=order[r]; order[r]=t; }

    printf("\n--- Starting Exam '%s' ---\n", ex->title);
    int score=0;
    for (int i=0;i<qc;i++) {
        Question *q = &examQs[order[i]];
        printf("\nQuestion %d: %s\n", i+1, q->text);
        int l1 = (int)strlen(q->opt[0]), l3 = (int)strlen(q->opt[2]);
        int leftMax = l1 > l3 ? l1 : l3;
        int colWidth = 3 + leftMax + 6;
        printf("1) %s", q->opt[0]); int cur = 3 + (int)strlen(q->opt[0]); for (int sp=cur; sp<colWidth; sp++) putchar(' ');
        printf("2) %s\n", q->opt[1]);
        printf("3) %s", q->opt[2]); cur = 3 + (int)strlen(q->opt[2]); for (int sp=cur; sp<colWidth; sp++) putchar(' ');
        printf("4) %s\n", q->opt[3]);
        int ans; printf("Your answer (1-4): "); while (scanf("%d", &ans) != 1) { printf("Enter 1-4: "); while (getchar()!='\n'); } while(getchar()!='\n');
        if (ans == q->correct) score++;
    }
    printf("\n--- Exam finished ---\n");
    printf("Student: %s | Exam: %s | Score: %d / %d\n", username, ex->title, score, qc);
}

/* -------------------- Authentication and menus -------------------- */
static void student_menu(const char *username) {
    while (1) {
        printf("\n--- STUDENT MENU ---\n1) Take Exam\n2) View Exam Schedule\n0) Logout\nChoose: ");
        int ch; if (scanf("%d",&ch)!=1) { while(getchar()!='\n'); continue; } while(getchar()!='\n');
        if (ch==0) { printf("Student logged out.\n"); break; }
        else if (ch==1) student_take_exam_flow(username);
        else if (ch==2) show_schedule_console();
        else printf("Invalid choice.\n");
    }
}

static void main_menu_login() {
    while (1) {
        printf("\n==== MAIN MENU ====\n1) Login\n0) Exit\nChoose: ");
        int opt; if (scanf("%d",&opt)!=1) { while(getchar()!='\n'); continue; } while(getchar()!='\n');
        if (opt==0) { printf("Exiting.\n"); exit(0); }
        if (opt==1) {
            char username[128], id[128]; int password;
            printf("Enter username (string): ");
            if (!fgets(username,sizeof(username),stdin)) continue; trim_newline(username);
            printf("Enter numeric password (integer): ");
            while (scanf("%d",&password)!=1) { printf("Enter integer: "); while(getchar()!='\n'); } while(getchar()!='\n');
            printf("Select role: 1) Student 2) Teacher\nChoose: ");
            int role; while (scanf("%d",&role)!=1) { printf("Enter 1 or 2: "); while(getchar()!='\n'); } while(getchar()!='\n');
            printf("Enter your ID (alphanumeric): ");
            if (!fgets(id,sizeof(id),stdin)) continue; trim_newline(id);
            if (role==1) {
                if (strcmp(id,"STUDENT-ID")!=0) { printf("Student ID mismatch (use STUDENT-ID for demo).\n"); continue; }
                printf("Welcome Student %s\n", username);
                student_menu(username);
            } else if (role==2) {
                if (strcmp(id,"TEACHER-ID")!=0) { printf("Teacher ID mismatch (use TEACHER-ID for demo).\n"); continue; }
                printf("Welcome Teacher %s\n", username);
                show_teacher_menu();
            } else printf("Invalid role.\n");
        } else printf("Invalid choice.\n");
    }
}

/* -------------------- Initialization & demo persistence -------------------- */
static void ensure_demo_and_load() {
    int qfile = file_exists(QUESTIONS_FILE);
    int efile = file_exists(EXAMS_FILE);

    if (!qfile) {
        load_demo_questions_memory(); // sets qcount and qbank[0..]
        for (int i=0;i<qcount;i++) save_question_to_file(&qbank[i]);
        printf("Demo questions created and saved to %s\n", QUESTIONS_FILE);
    } else {
        load_questions_from_file();
        printf("Loaded %d questions from %s\n", qcount, QUESTIONS_FILE);
    }

    if (!efile) {
        if (qcount>0) {
            Exam ex; ex.examId = 1; strcpy(ex.title,"Mid Term Exam"); ex.startTime = 1; ex.endTime = 10; ex.duration = 60;
            ex.qCount = (qcount >= 4) ? 4 : qcount;
            for (int i=0;i<ex.qCount;i++) ex.qids[i] = qbank[i].qid;
            ebank[ecount++] = ex;
            save_exam_to_file(&ex);
            printf("Demo exam created and saved to %s\n", EXAMS_FILE);
        }
    } else {
        load_exams_from_file();
        printf("Loaded %d exams from %s\n", ecount, EXAMS_FILE);
    }
}

/* -------------------- main -------------------- */
int main(void) {
    srand((unsigned)time(NULL));
    printf("Starting Online Exam System (persistent) ...\n");
    ensure_demo_and_load();
    main_menu_login();
    return 0;
}
