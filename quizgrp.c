/*
   +----------------------------------------------------------------------+
   | Программа для специальной группировки вопросов для викторин          |
   +----------------------------------------------------------------------+
   | 2009, 2019, http://github.com/vcsmutl1528/quizgrp                    |
   +----------------------------------------------------------------------+

  Утилита, посредством группировки вопросов, позволяет быстро найти глюки в базе: повторяющиеся вопросы,
ошибки при составлении вопросника, где буквы имеют неправильную раскладку клавиатуры, да и при 
ручном редактировании базы с такой группировкой дело идет куда быстрее.
  Программа понимает простейший входной формат базы вопросов в виде 'Вопрос|Ответ', где разделителем служит
трубопровод.

Формат командной строки:
	quizgrp <база> [<отчет>] [-l] [-q] [-a] [-u] [-s[1..90]]
Ключами в командной строке управляется, что включать в отчет:
  -l	- полностью совпадающие вопросы
  -q	- группировка по одинаковым вопросам, но разным ответам
  -a	- группировка по одинаковым ответам, в алфавитном порядке
  -s	- группировка по схожим вопросам (некоторые символы не совпадают).
		  1..90 - число в процентах от длины сравниваемых строк, регулирующее
		  допустимое количество несовпадающих символов (расстояние Левенштайна)
  При сравнении строк можно использовать специальное преобразование к общему виду, имеющему одинаковое
написание (например, латинская и кириллическая буквы o и o выглядят одинаково). Это задается ключом -u.
Кроме этого:
  -up	- игнорировать знаки препинания в конце вопроса, т.е. строки "вопрос.|ответ" и "вопрос?|ответ"
		  будут считаться одинаковыми
  -uq	- игнорировать кавычки в строке вопроса, т.е. строки вопрос|ответ "вопрос"|ответ будут
		  считаться одинаковыми
  Если хотя бы один из ключей -u, -up или -uq указан, то вопросник должен быть в кодировке CP-1251, так
как все коды преобразования русских букв и знаков препинания берутся из этой таблицы.
  Дополнительные ключи:
  -t<n> - число потоков для режима -s
  -d<l,d> - используемая текстовая метрика режима -s:
	-dl	- Левенштайна;	-dd	- Дамерау-Левенштайна
  -qstat - статистика по длине записей вопросника
  -v	- вывод дополнительных/отладочных сообщений
  Если отчет не указан, программа просканирует вопросник на наличие ошибок.
*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <locale.h>
#include <stdlib.h>
#include <io.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#define _WIN32_WINNT 0x0400
#include <windows.h>

#include "ccolor.h"
#include "crc32.h"

#define MAX_QUESTIONS 1048576				// Максимальное количество вопросов
#define MAX_STRING	2048					// Максимальная длина вопроса

#define ALLOCSIZE	(1024*1024*8)			// Начальный размер области для динамических структур
#define ALLOCSIZEINC	(1024*1024*16)		// Размер, на который она увеличивается если не хватило

#define QUEST_DELIMITER '|'					// Разделитель вопроса и ответа
#define PADLEN	100							// Длина шапки-разделителя в отчете
#define PERPARDEF	35						// Значение по умолчанию для -s

#define STATACCTIME (60 * CLOCKS_PER_SEC)		// Время накопления статистики времени для режима -s
#define STATREFRESHTIME (1 * CLOCKS_PER_SEC)	// Время обновления информации

#define BUF_SIZE 1024

#if PADLEN > MAX_STRING
#error PADLEN > MAX_STRING
#endif

#ifdef _DEBUG
#define F_VERBOSE 1
#else
#define F_VERBOSE 0
#endif

#if defined (_MSC_VER) && (_MSC_VER > 1200)
#define HAVE_INTRINSICS
#endif

#define COLOR_VERBOSE (FOREGROUND_INTENSITY)

#define LEVENSHTEIN_MAX_LENTH 255

#define MAXTHREADS MAXIMUM_PROCESSORS
#define MINSLICE		200

#define LEVDROWSIZE (LEVENSHTEIN_MAX_LENTH+1)

int levdp1a [MAXTHREADS][LEVDROWSIZE];
int levdp2a [MAXTHREADS][LEVDROWSIZE];
int levdp3a [MAXTHREADS][LEVDROWSIZE];

static char str [MAX_STRING];
static char buf [BUF_SIZE];

struct question
{
	int index;
	char *question;
	char *answer;
} quiz [MAX_QUESTIONS];

typedef struct questionl_tag
{
	char *question;
	char *answer;
	short qlen;
	short alen;
} questionl;

questionl *quizu;

typedef struct collthrd_ctx_tag
{
	int *levdp1;			// !смещение не должно меняться для levdist.asm
	int *levdp2;
	int *levdp3;
	HANDLE hThread;
	HANDLE hStart;
	HANDLE hFinish;
	int istart;
	int iend;
	int jstart;
	int jend;
	volatile int icur;
} collthrd_ctx;

collthrd_ctx threads [MAXTHREADS];

HANDLE hWaitObjects [MAXTHREADS+1];

typedef unsigned char bflag;
bflag f_l = 0;
bflag f_q = 0;
bflag f_a = 0;
bflag f_s = 0;
int f_sp = PERPARDEF;
bflag f_st = -1;
bflag f_qs = 0;
bflag f_m = 0;
bflag f_u = 0, f_nu = 0;
bflag f_up = 0, f_uq = 0;
bflag f_v = F_VERBOSE;
#if defined (_DEBUG) && defined (_MSC_VER) && (_MSC_VER < 1300)
bflag f_t = 1;
#else
bflag f_t = 0;
#endif

#if MAXTHREADS > UCHAR_MAX
#error MAXTHREADS > UCHAR_MAX
#endif

char *quizfile = NULL;
char *groupedfile = NULL;

char *quizs;
char *quizs_ptr;
int quizc;

typedef unsigned str_hash;

char xlat  [256];			// uniform-преобразование
char chumask [256];			// 0-бит признак кавычки (-uq), 1-бит - признак знака пунктуации (-up)

char *trim (char *s, int len, int *lentr, char mask[256]);
char *uniform (char *s);

__inline unsigned strhash (char *str) {
	return ~_crc32_update (str, strlen (str), ~0); }

typedef struct listnode_tag listnode;
typedef struct hashkey_tag hashkey;

typedef struct listnode_tag
{
	int	val;
	listnode *next;
} listnode;

typedef struct hashkey_tag
{
	str_hash hash;
	listnode *node;
	hashkey *vnext;
	hashkey *next;
} hashkey;

hashkey *prihash [65536];
hashkey *hashkeylist;
hashkey *lasthashkey;
hashkey *ahp;
int hashkeyc;
char *hashtop;
unsigned allocated;

listnode **clist;
listnode *clisttop;

int hash_create ();
int hash_push (str_hash h, int v);
void hash_free ();

int list_alloc ();
int list_collate (int i, int j);
void list_free ();

int list_alloc_mt ();
int list_collate_mt (int i, int j);
void list_free_mt ();

int __cdecl answer_cmp (const void *a1, const void *a2);				// Нужна для сортировки ответов

char *ctoatime (clock_t t);
char *ctoatimes (clock_t t);
clock_t tstat_acc = STATACCTIME;

void pbarinit (int max);
void pbar (int cur);
void pbarbr (void);

int __cdecl c_printf (const char *format, ...);
int c_puts (const char *s);
int __cdecl v_printf (const char *format, ...);

DWORD WINAPI collateThreadProc (LPVOID lpParam);
LARGE_INTEGER liDueTime;

int __stdcall levdist (const char *s1, int l1, const char *s2, int l2);
int __stdcall levdist_mt (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);
int __stdcall levdamdist (const char *s1, int l1, const char *s2, int l2);
int __stdcall levdamdist_mt (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);
extern int __stdcall levdist_asm (const char *s1, int l1, const char *s2, int l2);
extern int __stdcall levdist_mt_asm (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);
extern int __stdcall levdamdist_asm (const char *s1, int l1, const char *s2, int l2);
extern int __stdcall levdamdist_mt_asm (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);
extern int __stdcall levdamdist_asm2 (const char *s1, int l1, const char *s2, int l2);
extern int __stdcall levdamdist_mt_asm2 (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);

int (__stdcall *plevdist) (const char *s1, int l1, const char *s2, int l2);
int (__stdcall *plevdist_mt) (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);

int get_number_of_proc (void);
int is_cond_move_present (void);
int get_msvcrt_version (unsigned *m, unsigned *l);

int __fastcall eindexl (int istart, unsigned lsz);
void __fastcall qcollate (int i, int j);
void __fastcall qcollate_mt (int i, int j, collthrd_ctx *pctx);

unsigned int qlen_count [LEVDROWSIZE];

const char * const szMemError = "Ошибка при выделении памяти.\n";

#ifndef _CRTAPI1
#define _CRTAPI1 __cdecl
#endif

int _CRTAPI1 main (int argc, char *argv[])
{
	int i, j = 0, k;
	char *sc, *sc2;
	FILE *f, *fout = NULL;
	int strl, tr, fsize;
	char *qs, *as;
	int ql, al, c, cu;
	hashkey *p, **pa;
	listnode *l;
	bflag f_needq;

/* ---------------------------- Разбор параметров ----------------------------- */

	cclrinit ();
	crc32_init ();
	setlocale (LC_ALL, ".1251");
	{
		unsigned m, l;
		if (get_msvcrt_version (&m, &l))
			if (HIWORD (m) < 6)
				f_t = 1;
			else if (HIWORD (m) <= 7 && HIWORD (l) <= 2600)
				f_t = 1;

	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-' || argv[i][0] == '/')
		{
			sc = argv[i] + 1;
			if (!stricmp (sc, "qstat"))
				f_qs = 1; else
			while (*sc != '\0')
			{
				switch (*sc)
				{
				case 'l': f_l = 1; break;
				case 'q': f_q = 1; break;
				case 'a': f_a = 1; break;
				case 'u':
					if (sc[1] == 'p') f_up = 1, sc++;
					else if (sc[1] == 'q') f_uq = 1, sc++;
					else if (sc[1] == '-') f_nu = 1, sc++;
					else f_u = 1;
					break;
				case 's':
					f_s = 1;
					k = strspn (sc+1, "0123456789");
					if (k != 0)
						f_sp = atoi (sc+1);
					sc += k;
					break;
				case 't':
					k = strspn (sc+1, "0123456789");
					if (k != 0)
						f_st = atoi (sc+1);
					sc += k;
					break;
				case 'd':
					if (sc[1] == 'd') f_m = 2, sc++;
					else if (sc[1] == 'l') f_m = 1, sc++;
					break;
				case 'v':
					f_v = 1; break;
				default: c_printf ("Внимание: Неизвестный ключ '%c'.\n", *sc);
				}
				sc++;
			}
			continue;
		}
		switch (j)
		{
		case 0:
			quizfile = argv [i];
			break;
		case 1:
			groupedfile = argv [i];
			break;
		default:
			c_printf ("Внимание: Лишний параметр '%s' игнорирован.\n", argv[i]);
		}
		j++;
	}
	if (HIWORD (m) > 7)
		v_printf ("MSVCRT version > 7\n");
	}

	f_needq = f_l | f_q | f_a | f_s | f_qs;
	if (quizfile == NULL && !f_needq)
	{
		sc = strrchr (argv [0], '\\');
		if (sc == NULL)
			sc = argv [0];
		else	sc++;
		c_puts ("Программа для специальной группировки вопросов для викторин.\n");
		c_printf ("Формат: %s <вопросник> [<отчет>] [-u] [-l] [-q] [-a] [-s[1..90]]\n", sc);
		c_puts ("Содержание отчета:\n"
			"  -l\t- полностью совпадающие вопросы\n"
			"  -q\t- одинаковые вопросы, но разные ответы\n"
			"  -a\t- одинаковые ответы, в алфавитном порядке\n"
			"  -s\t- группировка по схожим вопросам\n"
			"\t  (некоторые символы не совпадают, очень долго)\n"
			"\t  [1..90] - число в процентах от длины сравниваемых строк,\n"
			"\t\t    регулирующее допустимое количество несовпадающих\n");
		c_printf ("\t\t    символов (расстояние Левенштайна, по умолчанию %d)\n", PERPARDEF);
		c_puts ("Сравнение строк:\n"
			"  -u\t- использовать специальное представление\n"
			"  -up\t- игнорировать знаки препинания в конце вопроса\n"
			"  -uq\t- игнорировать кавычки в строке вопроса\n"
			"Дополнительные ключи:\n"
			"  -t<n> - число потоков для режима -s\n"
			"  -d<l,d> - используемая текстовая метрика режима -s:\n"
			"\t-dl\t- Левенштайна;\t-dd\t- Дамерау-Левенштайна\n"
			"  -qstat - статистика по длине записей вопросника\n"
			"  -v\t- вывод дополнительных/отладочных сообщений\n"
			"Если отчет не указан, программа просканирует вопросник на наличие ошибок.\n");
		return 0;
	}

	if (!f_needq && groupedfile)
		f_l = f_q = f_a = f_needq = 1;

/* -------------------- Чтение вопросника с проверкой ----------------------------------------- */

	f = fopen (quizfile, "rb");
	if (f == NULL)
	{
		c_printf ("Невозможно открыть вопросник '%s'. %s", quizfile, _strerror(NULL));
		return 1;
	}

	if (groupedfile != NULL)
	{
		fout = fopen (groupedfile, "w");
		if (fout == NULL)
		{
			fclose (f);
			c_printf ("Ошибка при создании отчета '%s'. %s", groupedfile, _strerror(NULL));
			return 1;
		}
	}

	c_printf ("Чтение вопросника из файла '%s'...\n", quizfile);
	if (fout != NULL)
		fprintf (fout, "Чтение вопросника из файла '%s'.\n\n", quizfile);

	fsize = filelength (fileno(f));
	quizs = malloc (fsize);
	if (quizs == NULL)
	{
		c_printf ("Ошибка при выделении памяти (%d КБ).\n", fsize >> 10);
		fclose (f);
		if (fout != NULL)
			fclose (fout);
		return 1;
	}
	quizs_ptr = quizs;

	memset (xlat, 0, 256);
	xlat [' '] = xlat ['\n'] = xlat ['\r'] =
		xlat ['\t'] = xlat ['\v'] = xlat ['\0'] = 1;
	pbarinit (fsize);
	i = j = 0;
	quizc = 0;
	while (!feof (f))
	{
		i++;
		qs = fgets (str, MAX_STRING, f);
 		if (qs == NULL)	break;
		strl = strlen (str);
		j += strl;
		pbar (j);
		if (strl >= MAX_STRING-1 && str [strl-1] != '\n' && str [strl-1] != '\r') {
			if (fout) fprintf (fout, "#%d: Строка вопроса слишком длинная. Вопрос пропущен.\n", i);
			else {
				pbarbr ();
				c_printf ("#%d: Строка вопроса слишком длинная. Вопрос пропущен.\n", i);
			}
			while (!feof (f)) {
				qs = fgets (str, MAX_STRING, f);
 				if (qs == NULL)	break;
				strl = strlen (str);
				j += strl;
				if (strl < MAX_STRING-1) break;
			}
			if (feof (f)) break;
			continue;
		}
		while (str [strl-1] == '\n' || str [strl-1] == '\r')
			strl--;
		str [strl] = '\0';
		qs = trim (str, strl, &tr, xlat);
		if (tr == 0)
		{
			if (fout != NULL)
				fprintf (fout, "#%d: Строка пуста.\n", i);
			else
			{
				pbarbr ();
				c_printf ("#%d: Строка пуста.\n", i);
			}
			continue;
		}
		sc = strchr (str, QUEST_DELIMITER);
		if (sc == NULL)
		{
			if (fout != NULL)
				fprintf (fout, "#%d: Разделитель вопроса отсутствует (%s). Вопрос пропущен.\n", i, str);
			else
			{
				pbarbr ();
				c_printf ("#%d: Разделитель вопроса отсутствует. Вопрос пропущен.\n", i);
			}
			continue;
		}
		sc2 = strchr (sc+1, QUEST_DELIMITER);
		if (sc2 != NULL)
		{
			if (fout != NULL)
				fprintf (fout, "#%d: Наличие нескольких полей после разделителя (%s). Дополнительные поля игнорированы.\n", i, str);
			else
			{
				pbarbr ();
				c_printf ("#%d: Наличие нескольких полей после разделителя. Дополнительные поля игнорированы.\n", i);
			}
		} else	sc2 = qs + tr;
		qs = trim (str, sc - str, &ql, xlat);
		if (ql == 0)
		{
			if (fout != NULL)
				fprintf (fout, "#%d: Строка вопроса пуста (%s). Вопрос пропущен.\n", i, str);
			else
			{
				pbarbr ();
				c_printf ("#%d: Строка вопроса пуста. Вопрос пропущен.\n", i);
			}
			continue;
		}
		as = trim (sc+1, (sc2 - sc) - 1, &al, xlat);
		if (al == 0)
		{
			if (fout != NULL)
				fprintf (fout, "#%d: Строка ответа пуста (%s). Вопрос пропущен.\n", i, str);
			else
			{
				pbarbr ();
				c_printf ("#%d: Строка ответа пуста. Вопрос пропущен.\n", i);
			}
			continue;
		}
		if (ql < sc - str)
		{
			if (fout != NULL)
				fprintf (fout, "#%d: Вопрос содержит пробельные символы.\n", i);
			else
			{
				pbarbr ();
				c_printf ("#%d: Вопрос содержит пробельные символы.\n", i);
			}
		}
		if (al < (sc2 - sc) - 1)
		{
			if (fout != NULL)
				fprintf (fout, "#%d: Ответ содержит пробельные символы.\n", i);
			else
			{
				pbarbr ();
				c_printf ("#%d: Ответ содержит пробельные символы.\n", i);
			}
		}

		if (f_needq)
		{
			memcpy (quizs_ptr, qs, ql);
			quizs_ptr [ql] = '\0';
			quiz [quizc].question = quizs_ptr;
			quizs_ptr += ql+1;
			memcpy (quizs_ptr, as, al);
			quizs_ptr [al] = '\0';
			quiz [quizc].answer = quizs_ptr;
			quizs_ptr += al+1;
			quiz [quizc].index = i;
		}
		quizc ++;
		if (quizc >= MAX_QUESTIONS)
		{
			c_printf ("Число вопросов превысило максимальное значение %d.\n", MAX_QUESTIONS);
			break;
		}
	}
	if (ferror (f))
		c_printf ("Ошибка при чтении файла '%s'. %s.\n", quizfile, _strerror(NULL));
	fclose (f);
	pbarbr();
	c_printf ("%d вопросов прочитано. %d пропущено.\n", quizc, i - quizc);
	if (fout != NULL)
		fprintf (fout, "\n%d вопросов прочитано. %d пропущено.\n", quizc, i - quizc);
	if (!f_needq || quizc == 0)
	{
		if (fout) fclose (fout);
		c_printf ("Выполнено.\n");
		return 0;
	}
	if (!(f_nu | f_u | f_up | f_uq)) {
		f_u = f_up = f_uq = 1;
		c_puts ("Внимание: Использовано специальное представление строк (ключи -u -up -uq).\n"
			"  Для отмены укажите -u-.\n");
	}
	v_printf ("Использовано памяти для вопросника: %d КБ\n", (quizc * sizeof (struct question) +
		(quizs_ptr - quizs)) >> 10);

	if (f_u) {
		for (i=0; i<256; i++)
			xlat [i] = i;
		for (sc = "aeёkopcyx‘’“”«»‚„", sc2 = "аеекорсух\"\"\"\"\"\"\"\""; *sc; sc++, sc2++)
			xlat [(unsigned char)*sc] = *sc2;
	}
	if (f_up || f_uq)
		memset (chumask, 0, sizeof (chumask));
	if (f_up)
		for (sc = ",.'\"!?:;‘’“”«»…‚„"; *sc; sc++)			// знаки пунктуации в конце вопроса (ключ -up)
			chumask [(unsigned char)*sc] = 2;
	if (f_uq)
		for (sc = "'\"‘’“”«»‚„"; *sc; sc++)					// кавычки всех видов (-uq) (апостроф не надо?)
			chumask [(unsigned char)*sc] |= 1;

	v_printf ("Установленная локаль: %s\n", setlocale (LC_ALL, NULL));

/* -------------------- Поиск дублирующихся строк ----------------------------------------- */

	__try {
	if (f_l) {
		c_printf ("Поиск дублирующихся строк...\n");
		if (fout) {
			strcpy (str, "\nДублирующиеся строки: ");
			memset (str + strlen (str), '-', PADLEN - strlen (str));
			str [PADLEN] = '\0';
			fprintf (fout, "%s\n\n", str);
		}

		if (!hash_create ())
		{
			c_printf (szMemError);
			return 1;
		}

		__try {
		for (i=0; i<quizc; i++)
		{
			strcpy (str, quiz [i].question);
			strcat (str, quiz [i].answer);
			uniform (str);
			if (!hash_push (strhash (str), i))
			{
				c_printf (szMemError);
				return 1;
			}
		}

		c = cu = 0;
		for (p = hashkeylist; p != NULL; p = p->next)
			if (p->node->next != NULL)
			{
				cu++;
				for (l = p->node; l != NULL; l = l->next)
				{
					c++;
					if (fout) fprintf (fout, "#%d: %s|%s\n", quiz [l->val].index,
						quiz [l->val].question, quiz [l->val].answer);
				}
				if (fout) fprintf (fout, "\n");
			}
		if (fout)
			if (c != 0)
				fprintf (fout, "Всего дублирующихся строк: %d (%.2f%%)\n"
					"Уникальных: %d\n", c - cu, (float)(c - cu) / quizc * 100, cu);
			else	fprintf (fout, "(нет таких)\n");
		else if (c != 0)
				c_printf ("Всего дублирующихся строк: %d (%.2f%%)\n"
					"Уникальных: %d\n", c - cu, (float)(c - cu) / quizc * 100, cu);
			else	c_puts ("Дублирующихся строк не найдено.\n");

		v_printf ("Макс. памяти использовано: %d КБ\n",
			(quizc * sizeof (struct question) + (quizs_ptr - quizs) +
			(hashtop - (char*)hashkeylist)) >> 10);
		} __finally {
			hash_free ();
		}
	}

/* -------------------- Поиск дублирующихся вопросов ----------------------------------------- */

	if (f_q) {
		c_printf ("Поиск дублирующихся вопросов...\n");
		if (fout) {
			strcpy (str, "\nДублирующиеся вопросы: ");
			memset (str + strlen (str), '-', PADLEN - strlen (str));
			str [PADLEN] = '\0';
			fprintf (fout, "%s\n\n", str);
		}

		if (!hash_create ())
		{
			c_printf (szMemError);
			return 1;
		}

		__try {
		for (i=0; i<quizc; i++)
		{
			strcpy (str, quiz [i].question);
			uniform (str);
			if (!hash_push (strhash (str), i))
			{
				c_printf (szMemError);
				return 1;
			}
		}

		c = cu = 0;
		for (p = hashkeylist; p != NULL; p = p->next)
			if (p->node->next != NULL)
			{
				cu++;
				for (l = p->node; l != NULL; l = l->next)
				{
					c++;
					if (fout) fprintf (fout, "#%d: %s|%s\n", quiz [l->val].index,
						quiz [l->val].question, quiz [l->val].answer);
				}
				if (fout) fprintf (fout, "\n");
			}
		if (fout)
			if (c != 0)
				fprintf (fout, "Всего дублирующихся вопросов: %d (%.2f%%)\n"
					"Уникальных: %d\n", c - cu, (float)(c - cu) / quizc * 100, cu);
			else	fprintf (fout, "(нет таких)\n");
		else if (c != 0)
				c_printf ("Всего дублирующихся вопросов: %d (%.2f%%)\n"
					"Уникальных: %d\n", c - cu, (float)(c - cu) / quizc * 100, cu);
			else	fprintf (fout, "Дублирующихся вопросов не найдено.\n");

		v_printf ("Макс. памяти использовано: %d КБ\n",
			(quizc * sizeof (struct question) + (quizs_ptr - quizs) +
			(hashtop - (char*)hashkeylist)) >> 10);
		} __finally {
			hash_free ();
		}
	}

/* -------------------- Поиск дублирующихся ответов ----------------------------------------- */

	if (f_a) {
		c_printf ("Поиск одинаковых ответов...\n");
		if (fout) {
			strcpy (str, "\nОдинаковые ответы: ");
			memset (str + strlen (str), '-', PADLEN - strlen (str));
			str [PADLEN] = '\0';
			fprintf (fout, "%s\n\n", str);
		}

		if (!hash_create ())
		{
			c_printf (szMemError);
			return 1;
		}

		__try {
		for (i=0; i<quizc; i++)
		{
			strcpy (str, quiz [i].answer);
			uniform (str);
			if (!hash_push (strhash (str), i))
			{
				c_printf (szMemError);
				return 1;
			}
		}

		c = cu = 0;
		for (p = hashkeylist; p != NULL; p = p->next)
			if (p->node->next != NULL)
				cu ++;
		pa = malloc (cu * sizeof (hashkey*));
		if (pa == NULL)
		{
			c_printf (szMemError);
			return 1;
		}

		for (i = 0, p = hashkeylist; p != NULL; p = p->next)
			if (p->node->next != NULL)
				pa [i++] = p;

		c_printf ("Сортировка ответов...\n");
		qsort ((void*)pa, cu, sizeof (*pa), &answer_cmp);

		for (i=0; i<cu; i++)
		{
			for (l = pa[i]->node; l != NULL; l = l->next)
			{
				c++;
				if (fout) fprintf (fout, "#%d: %s|%s\n", quiz [l->val].index,
					quiz [l->val].question, quiz [l->val].answer);
			}
			if (fout) fprintf (fout, "\n");
		}
		if (fout) if (c != 0)
				fprintf (fout, "Всего одинаковых ответов: %d (%.2f%%)\n"
					"Всего вопросов с одинаковыми ответами: %d (%.2f%%)\n",
					cu, (float)cu / hashkeyc * 100, c, (float)c / quizc * 100);
			else	fprintf (fout, "(нет таких)\n");
		else if (c != 0)
				c_printf ("Всего одинаковых ответов: %d (%.2f%%)\n"
					"Всего вопросов с одинаковыми ответами: %d (%.2f%%)\n",
					cu, (float)cu / hashkeyc * 100, c, (float)c / quizc * 100);
			else	c_printf ("Вопросов с одинаковыми ответами не найдено.\n");

		v_printf ("Макс. памяти использовано: %d КБ\n",
			(quizc * sizeof (struct question) + (quizs_ptr - quizs) +
			(hashtop - (char*)hashkeylist) + hashkeyc * sizeof (hashkey*)) >> 10);

		free (pa);
		} __finally {
			hash_free ();
		}
	}

/* ---------------------------- Поиск схожих вопросов ----------------------------------------- */

	if (f_s) {
		clock_t cstart, cend;
		clock_t tstat_s, tstat_e, tstat_c, tstat_p;
		float tstat_sp, tstat_ep, tstat_cp;

		c_printf ("Поиск схожих вопросов...\n");
		if (f_sp <= 0 || f_sp >= 100)
		{
			f_sp = PERPARDEF;
			c_printf ("Внимание: Процентный параметр установлен на значение по умолчанию %d.\n", PERPARDEF);
		}
		i = is_cond_move_present ();
		switch (f_m) {
		case 1:
			plevdist = i ? levdist_asm : levdist;
			plevdist_mt = i ? levdist_mt_asm : levdist_mt;
			break;
		default:
			v_printf ("Bad text metric method number %u\n", f_m);
		case 0:
		case 2:
			plevdist = i ? levdamdist_asm : levdamdist;
			plevdist_mt = i ? levdamdist_mt_asm : levdamdist_mt;
			f_m = 2;
		}

		c_printf ("Используемая текстовая метрика: %s (%d%%)\n", f_m == 1 ? "Левенштайна" :
			f_m == 2 ? "Дамерау-Левенштайна" : "bad f_m metric number", f_sp);
		if (fout) {
			sprintf (str, "\nСхожие ответы (схожесть %d%%): ", f_sp);
			memset (str + strlen (str), '-', PADLEN - strlen (str));
			str [PADLEN] = '\0';
			fprintf (fout, "%s\n\n", str);
		}

		quizu = malloc (quizc * sizeof (questionl) + (quizs_ptr - quizs));
		if (quizu == NULL)
		{
			c_printf (szMemError);
			return 1;
		}
		sc = (char*) &quizu [quizc];
		for (i=0; i<quizc; i++)
		{
			strcpy (sc, quiz [i].question);
			uniform (sc);
			quizu [i].question = sc;
			strl = strlen (sc);
			quizu [i].qlen = strl;
			sc += strl + 1;
			strcpy (sc, quiz [i].answer);
			uniform (sc);
			quizu [i].answer = sc;
			strl = strlen (sc);
			quizu [i].alen = strl;
			sc += strl + 1;
		}

		f_sp = (f_sp << 16) / 100;

		i = get_number_of_proc ();
		if (f_st != (bflag)-1)
		{
			if (f_st <= 0 || f_st > MAXTHREADS)
			{
				c_printf ("Внимание: Число потоков установлено на число доступных процессоров (%d).\n", i);
				f_st = i;
			}
		}	else f_st = i;
		if (i != f_st)
			c_printf ("Внимание: Число потоков (%d) не соответствует числу доступных процессоров (%d).\n",
				f_st, i);

		if (f_st == 1)
		{
			if (!list_alloc ())
			{
				free (quizu);
				c_printf (szMemError);
				return 1;
			}
			tstat_s = tstat_e = tstat_p = cstart = clock ();
			tstat_sp = tstat_ep = 0.;
			tr = c_printf ("Осталось:           ( 0.0%%)"); tr -= 10;
			for (i=0; i<quizc-1; i++)
			{
				ql = quizu [i].qlen;
				if (ql <= LEVENSHTEIN_MAX_LENTH)
				for (j=i+1; j<quizc; j++)
					qcollate (i, j);
				tstat_c = clock ();
				if (tstat_c - tstat_p >= STATREFRESHTIME)
				{
					tstat_cp = (float) i * (2 * quizc - i - 1) / ((float) quizc * (quizc - 1));
					tstat_p = (clock_t)( (1.-tstat_cp)*(tstat_c-tstat_s)/(tstat_cp-tstat_sp) );
					if (tstat_c - tstat_e > tstat_acc)
					{
						if (tstat_s != tstat_e)
							tstat_s = tstat_e, tstat_sp = tstat_ep;
						tstat_e = tstat_c; tstat_ep = tstat_cp;
						tstat_acc = (tstat_c - cstart + tstat_p) >> 3;
					}
					while (tr-- > 0) printf ("\b");
					tr = c_printf ("%s  (%4.1f%%)\0\0", ctoatimes (tstat_p), tstat_cp * 100.);
					tstat_p = tstat_c;
				}
			}
			cend = clock ();
			cend -= cstart;
			c_printf ("\rВсего времени: %s  \n", ctoatime (cend));
		} else
		{
			float fslice, tslice, minslice;
			DWORD dwRes;

			if (!list_alloc_mt ())
			{
				free (quizu);
				c_printf (szMemError);
				return 1;
			}

			if (i > 1) c_puts ("Процессор многоядерный. ");
			c_printf ("Потоков: %d\n", f_st);

			for (i=0; i<f_st; i++)
			{
				threads [i].hStart = CreateEvent (NULL, FALSE, FALSE, NULL);
				if (!threads [i].hStart)
					break;
				threads [i].hFinish = hWaitObjects [i] = CreateEvent (NULL, FALSE, TRUE, NULL);
				if (!threads [i].hFinish)
				{
					CloseHandle (threads [i].hStart);
					break;
				}
				threads [i].hThread = CreateThread (NULL, 0, collateThreadProc, (LPVOID)&threads[i], 0, (LPDWORD)&k);
				if (!threads [i].hThread)
				{
					CloseHandle (threads [i].hFinish);
					CloseHandle (threads [i].hStart);
					break;
				}
				threads [i].levdp1 = levdp1a [i];
				threads [i].levdp2 = levdp2a [i];
				threads [i].levdp3 = levdp3a [i];
			}
			if (i != f_st || !(hWaitObjects [f_st] = CreateWaitableTimer (NULL, FALSE, NULL)))
			{
				for (i--; i>=0; i--)
				{
					CloseHandle (threads [i].hThread);
					CloseHandle (threads [i].hFinish);
					CloseHandle (threads [i].hStart);
				}
				free (quizu);
				c_printf ("Ошибка при создании объектов ядра.\n");
				return 1;
			}

			liDueTime.QuadPart = Int32x32To64 (STATREFRESHTIME, -10000);
			SetWaitableTimer (hWaitObjects [f_st], &liDueTime, 0, NULL, NULL, FALSE);
			tstat_s = tstat_e = tstat_p = cstart = clock ();
			tstat_sp = tstat_ep = 0.;
			c_printf ("Осталось:           ( 0.0%%)"); tr = 17;
			i = j = k = 0;
			c = f_st;
			fslice = 1.f;
			tslice = .5f / f_st;
			minslice = (float)MINSLICE * (MINSLICE - 1) / ((float)quizc * (quizc - 1));
			while (1)
			{
				dwRes = WaitForMultipleObjects (f_st+1, hWaitObjects, FALSE, INFINITE);
				if (dwRes == WAIT_FAILED)
					break;
				i = dwRes - WAIT_OBJECT_0;
				if (i == f_st)
				{
					int imin = quizc, ddq = 2 * quizc - 1;
					tstat_c = clock ();
					tstat_cp = 0.;
					for (i=0; i<f_st; i++)
					{
						if (threads [i].istart != threads [i].iend)
						{
							if (threads [i].istart < imin)
								imin = threads [i].istart;
							tstat_cp += (float)threads [i].icur * (ddq - threads [i].icur) -
								(float)threads [i].istart * (ddq - threads [i].istart);
						}
					}
					tstat_cp = (tstat_cp + (float) imin * (ddq - imin)) / ((float) quizc * (quizc - 1));
					tstat_p = (clock_t)( (1.-tstat_cp)*(tstat_c-tstat_s)/(tstat_cp-tstat_sp) );
					if (tstat_c - tstat_e > tstat_acc)
					{
						if (tstat_s != tstat_e)
							tstat_s = tstat_e, tstat_sp = tstat_ep;
						tstat_e = tstat_c; tstat_ep = tstat_cp;
						tstat_acc = (tstat_c - cstart + tstat_p) >> 3;
					}
					while (tr-- > 0) printf ("\b");
					tr = printf ("%s  (%4.1f%%)\0\0",	ctoatimes (tstat_p), tstat_cp * 100.);
					SetWaitableTimer (hWaitObjects [f_st], &liDueTime, 0, NULL, NULL, FALSE);
					continue;
				}
				c --;
				if (c == 0 && j == quizc)
					break;
				if (k == f_st)
				{
					k = 0;
					tslice *= 0.5;
					if (tslice < minslice)
						tslice = minslice;
				}
				threads [i].istart = threads [i].icur = j;
				k ++;
				fslice -= tslice;
				if (fslice < 0.)
					fslice = 0.;
				j = threads [i].iend = (2 * quizc - (int)sqrt((float)(4 * quizc) * (quizc - 1) * fslice + 1.) - 1) / 2 + 1;
				if (threads [i].istart != threads [i].iend && threads [i].istart != quizc)
				{
					SetEvent (threads [i].hStart);
					c ++;
				}
			}
			cend = clock ();
			cend -= cstart;
			c_printf ("\rВсего времени: %s  \n", ctoatime (cend));

			CloseHandle (hWaitObjects [f_st]);
			for (i=0; i<f_st; i++)
			{
				TerminateThread (threads [i].hThread, 0);
				CloseHandle (threads [i].hThread);
				CloseHandle (threads [i].hFinish);
				CloseHandle (threads [i].hStart);
			}
		}

		c = cu = 0;
		for (i=0; i<quizc; i++)
			if (clist [i] != NULL)
			{
				cu ++;
				for (l = clist [i]; l; l = l->next)
				{
					c ++;
					if (fout) fprintf (fout, "#%d: %s|%s\n", quiz [l->val].index,
						quiz [l->val].question, quiz [l->val].answer);
					clist [l->val] = NULL;
				}
				if (fout) fprintf (fout, "\n");
			}
		if (fout) if (c != 0)
				fprintf (fout, "Всего схожих вопросов: %d (%.2f%%)\n"
					"Схожих групп: %d\n", c, (float)c / quizc * 100, cu);
			else	fprintf (fout, "(нет таких)\n");
		else if (c != 0)
				c_printf ("Всего схожих вопросов: %d (%.2f%%)\n"
					"Схожих групп: %d\n", c, (float)c / quizc * 100, cu);
			else	c_puts ("Схожих вопросов для заданной текстовой метрики не найдено.\n");

		if (f_st == 1)
			list_free ();
		else
			list_free_mt ();
		free (quizu);
	}
	if (f_qs) {
		unsigned int quizq_len;

		c_printf ("Подсчет статистики по длине записей...\n");
		if (fout) {
			strcpy (str, "\nСтатистика по длине записей вопросника: ");
			memset (str + strlen (str), '-', PADLEN - strlen (str));
			str [PADLEN] = '\0';
			fprintf (fout, "%s\n\n", str);
		}

		memset (qlen_count, 0, sizeof (qlen_count[0]) * LEVDROWSIZE);
		quizq_len = 0;
		for (i = 0; i < quizc; i++) {
			strl = strlen (quiz [i].question);
			if (strl == 0) continue;
			qlen_count [strl-1] ++;
			quizq_len += strl;
		}

		for (i = 0; i < LEVENSHTEIN_MAX_LENTH; i+=16) {
			c = 0;
			for (j = i; j < i + 16 && j < LEVENSHTEIN_MAX_LENTH; j++)
				c += qlen_count [j];
			j = _snprintf (str, MAX_STRING-1, "%3d - %3d: %u\n", i+1,
				i+(16-1) < LEVENSHTEIN_MAX_LENTH ? i+16 : LEVENSHTEIN_MAX_LENTH, c);
			if (j < 0 || j >= MAX_STRING-1)
				str [MAX_STRING - 1] = '\0';
			if (fout)
				fputs (str, fout);
			else
				c_puts (str);
		}
		for (i = 0; i < LEVENSHTEIN_MAX_LENTH; i++)
			if (fout)
				fprintf (fout, "%3d: %3u\n", i+1, qlen_count [i]);
			else
				c_printf ("%3d: %3u\n", i+1, qlen_count [i]);
		if (fout) fprintf (fout, "Размер вопросника в памяти: %u\n", quizq_len);
	}
	} __finally {
		free (quizs);
		if (fout) fclose (fout);
	}

	return 0;
}

int __fastcall eindexl (int istart, unsigned lsz)
{
	unsigned sz = 0;
	while (sz <= lsz && istart < quizc)
	{
		sz += quizu [istart].qlen + quizu [istart].alen + 2;
		istart++;
	}
	return istart;
}

void __fastcall qcollate (int i, int j)
{
	int qli = quizu [i].qlen, qlj = quizu [j].qlen, strl, k;

	if (qlj > LEVENSHTEIN_MAX_LENTH)
		return;
	if (clist [i] != NULL && clist [i] == clist [j])
		return;
	strl = min (qli, qlj) * f_sp >> 16;
	if (strl == 0 || abs (qlj - qli) > strl)
		return;
	k = plevdist (quizu [i].question, qli, quizu [j].question, qlj);

	if (k != 0 && k <= strl)
		list_collate (i, j);
}

void __fastcall qcollate_mt (int i, int j, collthrd_ctx *pctx)
{
	int qli = quizu [i].qlen, qlj = quizu [j].qlen, strl, k;

	if (qlj > LEVENSHTEIN_MAX_LENTH)
		return;
	if (clist [i] != NULL && clist [i] == clist [j])
		return;
	strl = min (qli, qlj) * f_sp >> 16;
	if (strl == 0 || abs (qlj - qli) > strl)
		return;
	k = plevdist_mt (quizu [i].question, qli, quizu [j].question, qlj, pctx);
	if (k != 0 && k <= strl)
		list_collate_mt (i, j);
}

CRITICAL_SECTION csclistmt;

DWORD WINAPI collateThreadProc (LPVOID lpParam)
{
	collthrd_ctx *pctx = (collthrd_ctx*)lpParam;
	int i, j;

	while (1)
	{
		WaitForSingleObject (pctx->hStart, INFINITE);
		for (i=pctx->istart; i<pctx->iend; i++)
		{
			if (quizu [i].qlen <= LEVENSHTEIN_MAX_LENTH)
				for (j=i+1; j<quizc; j++)
					qcollate_mt (i, j, pctx);
			pctx->icur = i;
		}
		SetEvent (pctx->hFinish);
	}
}

int __stdcall levdist (const char *s1, int l1, const char *s2, int l2)
{
	int *p1, *p2, *tmp;
	int i1, i2, c0, c1, c2;
	
	if (l1==0) return l2;
	if (l2==0) return l1;

	if (l1>LEVENSHTEIN_MAX_LENTH || l2>LEVENSHTEIN_MAX_LENTH)
		return -1;

	p1 = levdp1a[0];
	p2 = levdp2a[0];

	for (i2=0; i2<=l2; i2++)
		p1 [i2] = i2;

	for (i1=0; i1<l1; i1++)
	{
		p2 [0] = p1 [0] + 1;
		for (i2=0; i2<l2; i2++)
		{
			c0 = p1 [i2] + (s1[i1] == s2[i2] ? 0 : 1);
			c1 = p1 [i2+1] + 1;
			if (c1<c0) c0 = c1;
			c2 = p2 [i2] + 1;
			if (c2<c0) c0 = c2;
			p2 [i2+1] = c0;
		}
		tmp=p1; p1=p2; p2=tmp;
	}

//	c0 = p1 [l2];

	return c0;
}

int __stdcall levdist_mt (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx)
{
	int *p1, *p2, *tmp;
	int i1, i2, c0, c1, c2;
	
	if (l1==0) return l2;
	if (l2==0) return l1;

	if (l1>LEVENSHTEIN_MAX_LENTH || l2>LEVENSHTEIN_MAX_LENTH)
		return -1;

	p1 = pctx->levdp1;
	p2 = pctx->levdp2;

	for (i2=0; i2<=l2; i2++)
		p1 [i2] = i2;

	for (i1=0; i1<l1; i1++)
	{
		p2 [0] = p1 [0] + 1;
		for (i2=0; i2<l2; i2++)
		{
			c0 = p1 [i2] + (s1[i1] == s2[i2] ? 0 : 1);
			c1 = p1 [i2+1] + 1;
			if (c1<c0) c0 = c1;
			c2 = p2 [i2] + 1;
			if (c2<c0) c0 = c2;
			p2 [i2+1] = c0;
		}
		tmp=p1; p1=p2; p2=tmp;
	}

//	c0 = p1 [l2];

	return c0;
}

int __stdcall levdamdist (const char *s1, int l1, const char *s2, int l2)
{
	int *row0 = levdp1a [0];
	int *row1 = levdp2a [0];
	int *row2 = levdp3a [0];
	int i, j;

	for (j = 0; j <= l2; j++)
		row1[j] = j;
	for (i = 0; i < l1; i++) {
		int *dummy;

		row2[0] = i + 1;
		for (j = 0; j < l2; j++) {
			/* substitution */
			row2[j + 1] = row1[j] + (s1[i] != s2[j] ? 1 : 0);
			/* swap */
			if (i > 0 && j > 0 && s1[i - 1] == s2[j] &&
					s1[i] == s2[j - 1] &&
					row2[j + 1] > row0[j - 1] + 1)
				row2[j + 1] = row0[j - 1] + 1;
			/* deletion */
			if (row2[j + 1] > row1[j + 1] + 1)
				row2[j + 1] = row1[j + 1] + 1;
			/* insertion */
			if (row2[j + 1] > row2[j] + 1)
				row2[j + 1] = row2[j] + 1;
		}

		dummy = row0;
		row0 = row1;
		row1 = row2;
		row2 = dummy;
	}

	i = row1[l2];

	return i;
}

int __stdcall levdamdist_mt (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx)
{
	int *row0 = pctx->levdp1;
	int *row1 = pctx->levdp2;
	int *row2 = pctx->levdp3;
	int i, j;

	for (j = 0; j <= l2; j++)
		row1[j] = j;
	for (i = 0; i < l1; i++) {
		int *dummy;

		row2[0] = i + 1;
		for (j = 0; j < l2; j++) {
			/* substitution */
			row2[j + 1] = row1[j] + (s1[i] != s2[j] ? 1 : 0);
			/* swap */
			if (i > 0 && j > 0 && s1[i - 1] == s2[j] &&
					s1[i] == s2[j - 1] &&
					row2[j + 1] > row0[j - 1] + 1)
				row2[j + 1] = row0[j - 1] + 1;
			/* deletion */
			if (row2[j + 1] > row1[j + 1] + 1)
				row2[j + 1] = row1[j + 1] + 1;
			/* insertion */
			if (row2[j + 1] > row2[j] + 1)
				row2[j + 1] = row2[j] + 1;
		}

		dummy = row0;
		row0 = row1;
		row1 = row2;
		row2 = dummy;
	}

	i = row1[l2];

	return i;
}

int __cdecl answer_cmp (const void *a1, const void *a2)
{
	return _stricoll (quiz [(*(hashkey**)a1)->node->val].answer,
		quiz [(*(hashkey**)a2)->node->val].answer);
}

int __cdecl c_printf (const char *format, ...)
{
	int r;
	va_list argptr;

	va_start (argptr, format);
	if (f_t) {
		r = _vsnprintf (buf, BUF_SIZE-1, format, argptr);
		if (r == BUF_SIZE-1)
			buf [BUF_SIZE-1] = '\0', r++;
		CharToOemA (buf, buf);
		if (fwrite (buf, r, 1, stdout) != 1) r = 0;
	} else
		r = vprintf (format, argptr);
	va_end (argptr);
	return r;
}

int c_puts (const char *s)
{
	size_t l = strlen (s);
	int r;

	if (f_t) {
		if (l >= BUF_SIZE)
			l = BUF_SIZE - 1, buf [BUF_SIZE-1] = '\0';
		CharToOemBuffA (s, buf, l);
		if (fwrite (buf, l, 1, stdout) == 1) r = l;
	} else
		if (fwrite (s, l, 1, stdout) == 1) r = l;

	return r;
}

char *trim (char *s, int len, int *lentr, char mask[256])
{
	while (len>0 && mask[(unsigned char)*s]) s++, len--;
	if (lentr != NULL)
	{
		while (len>0 && mask[(unsigned char)s[len-1]]) len--;
		*lentr = len;
	}

	return s;
}

char *uniform (char *s)
{
	char *p;

	if (f_up)
	{
		p = s + strlen (s);
		while (chumask [(unsigned char)p[-1]] & 2)
			p--;
		*p = '\0';
	}
	strlwr (s);						// работатет благодаря корректно установленной локали setlocale()
	if (f_uq)
	{
		char *ps = s;
		if (f_u)
		{
			for (p=s; *p; p++)
				if (!(chumask [(unsigned char)*p] & 1))
					*ps++ = xlat [(unsigned char)*p];
		} else
			for (p=s; *p; p++)
				if (!(chumask [(unsigned char)*p] & 1))
					*ps++ = *p;
		*ps = '\0';
	} else if (f_u)
		for (p=s; *p; p++)
			*p = xlat [(unsigned char)*p];
	return s;
}

int hash_reserve (size_t s);
hashkey *hash_add_new (str_hash h, int v);

int hash_create ()
{
	hashkeylist = malloc (ALLOCSIZE);
	if (hashkeylist == NULL)
		return 0;
	hashtop = (char*)hashkeylist;
	allocated = ALLOCSIZE;
	memset (prihash, 0, sizeof (prihash));
	lasthashkey = NULL;
	hashkeyc = 0;

	return 1;
}

int hash_push (str_hash h, int v)
{
	hashkey *ph;
	listnode *l, *llink;

	ahp = prihash [(unsigned short)h];
	if (ahp == NULL)
	{
		ahp = hash_add_new (h, v);
		if (ahp == NULL)
			return 0;
		prihash [(unsigned short)h] = ahp;
		return 1;
	}
	do {
		if (ahp->hash == h)
		{
			if (!hash_reserve (sizeof (listnode)))
				return 0;
			l = (listnode*) hashtop;
			l->val = v;
			l->next = NULL;
			hashtop += sizeof (listnode);
			llink = ahp->node;
			while (llink->next != NULL)
				llink = llink->next;
			llink->next = l;
			return 1;
		}
		if (ahp->vnext == NULL)
			break;
		ahp = ahp->vnext;
	} while (1);
	ph = hash_add_new (h, v);
	if (ph == NULL)
		return 0;
	ahp->vnext = ph;
	return 1;
}

hashkey *hash_add_new (str_hash h, int v)
{
	hashkey *p;

	if (!hash_reserve (sizeof (hashkey) + sizeof (listnode)))
		return NULL;
	p = (hashkey*) hashtop;
	p->hash = h;
	p->vnext = NULL;
	p->next = NULL;
	if (lasthashkey != NULL)
		lasthashkey->next = p;
	lasthashkey = p;
	hashtop += sizeof (hashkey);
	p->node = (listnode*) hashtop;
	hashtop += sizeof (listnode);
	p->node->val = v;
	p->node->next = NULL;
	hashkeyc++;

	return p;
}

int hash_reserve (size_t s)
{
	void *a;
	unsigned i, diff;
	hashkey *p;
	listnode *l;

	if (allocated - (hashtop - (char*)hashkeylist) >= s)
		return 1;
	a = realloc (hashkeylist, allocated + ALLOCSIZEINC);
	if (a == NULL)
		return 0;
	allocated += ALLOCSIZEINC;
	diff = (char*)a - (char*)hashkeylist;
	hashtop += diff;
	if (lasthashkey != NULL)
		lasthashkey = (hashkey*)((char*)lasthashkey + diff);
	ahp = (hashkey*)((char*)ahp + diff);
	hashkeylist = (hashkey*)a;
	for (p = hashkeylist; p != NULL; p = p->next)				// коррекция всех указателей в хеш-таблицах и связанных списках
	{
		if (p->vnext != NULL)
			p->vnext = (hashkey*)((char*)p->vnext + diff);
		p->node  = (listnode*)((char*)p->node + diff);
		for (l = p->node; l->next != NULL; l = l->next)
			l->next = (listnode*)((char*)l->next + diff);
		if (p->next != NULL)
			p->next = (hashkey*)((char*)p->next + diff);
	}

	for (i=0; i<65536; i++)
		if (prihash [i] != NULL)
			prihash [i] = (hashkey*)((char*) prihash[i] + diff);
	return 1;
}

void hash_free ()
{
	free (hashkeylist);
}

int list_alloc ()
{
	clist = malloc (quizc * sizeof (listnode*) + quizc * sizeof (listnode));
	if (clist == NULL)
		return 0;
	memset (clist, 0, quizc * sizeof (listnode*));

	clisttop = (listnode*)&clist [quizc];
	return 1;
}

int list_alloc_mt ()
{
	if (!list_alloc ())
		return 0;
	InitializeCriticalSection (&csclistmt);
	return 1;
}

int list_collate (int i, int j)
{
	listnode *l;

	if (clist [i] == NULL && clist [j] == NULL)
	{
		clist [i] = clisttop;
		clist [j] = clisttop;
		clisttop->val = i;
		clisttop->next = clisttop+1;
		clisttop++;
		clisttop->val = j;
		clisttop->next = NULL;
		clisttop++;
		return 1;
	}

	if (clist [i] != NULL && clist [j] == NULL ||
		clist [i] == NULL && clist [j] != NULL)
	{
		if (clist [j] == NULL)
		{
			clist [j] = l = clist [i];
			i = j;
		} else
			clist [i] = l = clist [j];
		while (l->next != NULL)
			l = l->next;
		l->next = clisttop;
		clisttop->val = i;
		clisttop->next = NULL;
		clisttop++;
		return 1;
	}

	if (clist [i] == clist [j])
		return 1;
	for (l = clist [i]; l->next != NULL; l = l->next);
	l->next = clist [j];
	for (l = clist [j]; l; l = l->next)
		clist [l->val] = clist [i];
	return 1;
}

int list_collate_mt (int i, int j)
{
	int r;
	EnterCriticalSection (&csclistmt);
	r = list_collate (i, j);
	LeaveCriticalSection (&csclistmt);
	return r;
}

void list_free ()
{
	free (clist);
}

void list_free_mt ()
{
	list_free ();
	DeleteCriticalSection (&csclistmt);
}

char ctoatimestr [15];

char *ctoatime (clock_t t)
{
	sprintf (ctoatimestr, "%02u:%02d:%02d.%03d",
		(unsigned long)t / (3600*CLOCKS_PER_SEC), t % (3600*CLOCKS_PER_SEC) / (60*CLOCKS_PER_SEC),
		t % (60*CLOCKS_PER_SEC) / CLOCKS_PER_SEC, t % CLOCKS_PER_SEC);
	return ctoatimestr;
}

char *ctoatimes (clock_t t)
{
	char *r;

	ctoatime (t);
	r = strrchr (ctoatimestr, '.');
	if (r != NULL)
		*r = '\0';
	return ctoatimestr;
}

int get_number_of_proc (void)
{
	SYSTEM_INFO si;

	GetSystemInfo (&si);

	return si.dwNumberOfProcessors;
}

int pbar_max, pbar_prev, pbar_outp;

void pbarinit (int max)
{
	pbar_max = max;
	pbar_prev = 0;
	printf ("0%%");
	pbar_outp = 1;
}

void pbar (int cur)
{
	int prev_percent, cur_percent, i;

	if (pbar_max <= 0 || pbar_prev >= pbar_max)
	{
		cur_percent = 10;
		if (pbar_max <= 0 && pbar_prev == 0)
			prev_percent = 0;
		else	prev_percent = 10;
	} else
	{
		prev_percent = pbar_prev * 10 / pbar_max;
		cur_percent = cur * 10 / pbar_max;
	}

	if (cur_percent > prev_percent)
	{
		for (i=prev_percent+1; i<=cur_percent; i++)
			printf ("....%d%%", i * 10);
		pbar_outp = 1;
	}
	if (cur_percent >= 10)
		pbar_prev = pbar_max <= 0 ? 1 : pbar_max;
	else	pbar_prev = cur;
}

void pbarbr (void)
{
	if (pbar_outp)
		printf ("\n");
	pbar_outp = 0;
}

int v_printf (const char *format, ...)
{
	int r;
	va_list argptr;
	unsigned char c;

	if (!f_v) return 0;
	va_start (argptr, format);
	c = csetclr (COLOR_VERBOSE);
	if (f_t) {
		r = _vsnprintf (buf, BUF_SIZE-1, format, argptr);
		if (r >= BUF_SIZE-1)
			buf [BUF_SIZE-1] = '\0', r = BUF_SIZE-1;
		CharToOemA (buf, buf);
		if (fwrite (buf, r, 1, stdout) != 1) r = 0;
	} else
		r = vprintf (format, argptr);
	csetclrv (c);
	va_end (argptr);
	return r;
}

static int check_cpuid_presence_386 (void);
#ifndef HAVE_INTRINSICS
static void __cpuid (unsigned CPUInfo [4], int InfoType);
static void __cpuidex (unsigned CPUInfo [4], int InfoType, int ECXValue);
#endif

#define CMOV_FLAG	0x00008000

int is_cond_move_present (void)
{
	unsigned CPUInfo [4];
	if (!check_cpuid_presence_386 ())
		return 0;
	__cpuid (CPUInfo, 1);
	if (CPUInfo [3] & CMOV_FLAG)
		return 1;
	return 0;
}

int __declspec(naked) check_cpuid_presence_386 (void)
{
	__asm {
		pushfd
		pop	eax
		xor	eax, 1 SHL 21
		push	eax
		popfd
		pushfd
		pop	edx
		xor	eax, edx
		test	eax, NOT (1 SHL 21)
		mov	eax, 1
		jz	short rt
		dec	eax
	rt:
		ret
	}
}

#ifndef HAVE_INTRINSICS
void __cpuid (unsigned CPUInfo [4], int InfoType)
{
	__cpuidex (CPUInfo, InfoType, 0);
}

void __declspec(naked) __stdcall __cpuidex (unsigned CPUInfo [4], int InfoType, int ECXValue)
{
	__asm {
		push ebp
		mov ebp, esp
		push ebx
		push edi
		mov eax, InfoType
		mov ecx, ECXValue
		_emit 0x0f
		_emit 0xa2
		mov edi, CPUInfo
		mov dword ptr [edi][ 0], eax
		mov dword ptr [edi][ 4], ebx
		mov dword ptr [edi][ 8], ecx
		mov dword ptr [edi][12], edx
		pop edi
		pop ebx
		pop ebp
		ret 12
	}
}
#endif

static char *szMsvcrt = "msvcrt.dll";
static const WCHAR *szVSVersionInfo = L"VS_VERSION_INFO";

int get_msvcrt_version (unsigned *m, unsigned *l)
{
	DWORD dw;
	LPVOID lpData, p;
	VS_FIXEDFILEINFO *pffi;
	WORD w;

	if (!m && !l) return 0;
	dw = GetFileVersionInfoSizeA (szMsvcrt, &dw);
	if (!dw)
		return 0;
	lpData = malloc (dw);
	if (!lpData)
		return 0;
	if (!GetFileVersionInfoA (szMsvcrt, 0, dw, lpData)) {
		free (lpData);
		return 0;
	}
	p = lpData;
	w = *(WORD*)p;				// wLength
	p = (char*)p + sizeof (WORD);
	if (w >= 6 + 34 + 16) {
	w = *(WORD*)p;			// wValueLength
	p = (char*)p + sizeof (WORD) + sizeof (WORD);	// wValueLength, wType
	if (w >= 16)
	if (!wcscmp (p, szVSVersionInfo))
	if (*(WORD*)((char*)p + 32) == 0) {
	pffi = (VS_FIXEDFILEINFO*)((char*)p + 34);
	if (pffi->dwSignature == VS_FFI_SIGNATURE) {
	if (m) *m = pffi->dwFileVersionMS;
	if (l) *l = pffi->dwFileVersionLS;
	return 1;
	} } }
	return 0;
}
