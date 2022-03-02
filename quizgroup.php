<?php

/*
   +----------------------------------------------------------------------+
   | Скрипт для специальной группировки вопросов для викторин             |
   +----------------------------------------------------------------------+
   | https://github.com/vcsmutl1528/quizgrp                               |
   +----------------------------------------------------------------------+

cp-1251

Скрипт посредством группировки вопросов позволяет быстро найти глюки в базе: повторяющиеся вопросы,
ошибки при составлении вопросника, где буквы имеют неправильную раскладку клавиатуры, да и при 
ручном редактировании базы с такой группировкой дело идет куда быстрее.
Скрипт понимает простейший входной формат базы вопросов в виде 'Вопрос|Ответ', где разделителем служит
трубопровод. База загружается в память целиком, так что проследите, чтоб ее хватило (Sclex base v2.5
7 МБ текста весит в памяти 40 МБ). Плюс размер внутренних структур, итого может дойти до 70 МБ.
Формат командной строки:
	quizgroup <база> [<отчет>] [-u] [-l] [-q] [-a]
Ключами в командной строке управляется, что включать в отчет:
  -l	- полностью совпадающие вопросы
  -q	- группировка по одинаковым вопросам, но разным ответам
  -a	- группировка по одинаковым ответам, в алфавитном порядке
При сравнении строк можно использовать специальное преобразование к общему виду, имеющему одинаковое
написание (например, буквы o и o выглядят одинаково). Это задается ключом -u, его желательно всегда
указывать.

*/

$f_u = false;
$f_l = false;
$f_q = false;
$f_a = false;

$quizfile = false;
$groupedfile = false;

/* ---------------------- Разбор параметров --------------------------------------- */

$ni = 0;
for ($i=1; $i<$argc; $i++)
{
	if (substr ($argv[$i], 0, 1) == '-' || substr ($argv[$i], 0, 1) == '/')
	{
		$sc = substr ($argv[$i], 1);
		while ($sc != '')
		{
			switch (substr ($sc, 0, 1))
			{
			case 'l': $f_l = true; break;
			case 'q': $f_q = true; break;
			case 'a': $f_a = true; break;
			case 'u': $f_u = true; break;
			default: echo "Warning: Unknown key '", substr($sc,0,1), "'.\n";
			}
			$sc = substr ($sc, 1);
		}
		continue;
	}
	switch ($ni)
	{
	case 0:
		$quizfile = $argv [$i];
		break;
	case 1:
		$groupedfile = $argv [$i];
		break;
	default:
		echo "Warning: Extra option '${argv[$i]}' ignored.\n";
	}
	$ni ++;
}
unset ($ni, $sc);

if ($f_l == false && $f_q == false && $f_a == false)
	$f_l = $f_q = $f_a = true;

if ($quizfile === false)
{
	echo "Questions grouping utility for quiz bases. 2009\n  https://github.com/vcsmutl1528/quizgrp\n";
	echo "Usage: ", basename($argv[0]), " <quiz-file> [<grouped-file>] [-u] [-l] [-q] [-a]\n";
	echo "  -u\t- use uniform string representation\n";
	echo "Include in report file:\n";
	echo "  -l\t- duplicated lines\n";
	echo "  -q\t- duplicated questions\n";
	echo "  -a\t- sorted list with equal answers\n";
	echo "If <groped-file> is not set, tool will scan quiz file for errors.\n";
	exit(0);
}

/* -------------------- Чтение вопросника с проверкой ----------------------------------------- */

$f = fopen ($quizfile, 'rb');
if ($f === false)
{
	echo "Failed to open questions file '$quizfile'.\n";
	exit(1);
}

$fout = false;
if ($groupedfile !== false)
{
	$fout = fopen ($groupedfile, 'w');
	if ($fout === false)
	{
		echo "Failed to open output file '$groupedfile'.\n";
		fclose($f);
		exit(1);
	}
}

echo "Reading questions from '$quizfile'...\n";
if ($fout !== false)
	fwrite ($fout, "Чтение вопросника из файла '$quizfile'.\n\n");

$quizq = array ();		// вопрос
$quiza = array ();		// ответ
$quizi = array ();		// номер строки в файле вопросника
$quizc = 0;			// количество правильно прочитанных вопросов

pbarinit (filesize ($quizfile));
$i = 0;
$read = 0;
while (!feof($f))
{
	$i++;
	$s = fgets ($f);
	$read += strlen ($s);
	pbar ($read);
	$s = rtrim ($s, "\n\r");
	$strim = trim ($s);
	if (strlen ($strim) == 0)
	{
		if ($fout !== false)
			fwrite ($fout, "#$i: Строка пуста.\n");
		else
		{
			pbarbr();
			echo "#$i: Question is empty.\n";
		}
		continue;
	}
	$ar = explode ('|', $s);
	if (count ($ar) < 2)
	{
		if ($fout !== false)
			fwrite ($fout, "#$i: Разделитель вопроса отсутствует ($s). Вопрос пропущен.\n");
		else
		{
			pbarbr();
			echo "#$i: Question delimiter is absent. Skipped...\n";
		}
		continue;
	}
	if (count ($ar) > 2)
	{
		if ($fout !== false)
			fwrite ($fout, "#$i: Наличие нескольких полей после разделителя ($s). Дополнительные поля игнорированы.\n");
		else
		{
			pbarbr();
			echo "#$i: Extra question delimiters. Extra fields ignored...\n";
		}
	}
	$q = $ar [0];
	$a = $ar [1];
	$qtrim = trim ($q);
	if (strlen ($qtrim) == 0)
	{
		if ($fout != false)
			fwrite ($fout, "#$i: Строка вопроса пуста ($s). Вопрос пропущен.\n");
		else
		{
			pbarbr();
			echo "#$i: Question is empty. Skipped...\n";
		}
		continue;
	}
	$atrim = trim ($a);
	if (strlen ($atrim) == 0)
	{
		if ($fout != false)
			fwrite ($fout, "#$i: Строка ответа пуста ($s). Вопрос пропущен.\n");
		else
		{
			pbarbr();
			echo "#$i: Answer is empty. Skipped...\n";
		}
		continue;
	}
	if (strlen ($qtrim) < strlen ($q))
	{
		if ($fout != false)
			fwrite ($fout, "#$i: Вопрос содержит пробельные символы.\n");
		else
		{
			pbarbr();
			echo "#$i: Question has space chars.\n";
		}
	}
	if (strlen ($atrim) < strlen ($a))
	{
		if ($fout != false)
			fwrite ($fout, "#$i: Ответ содержит пробельные символы.\n");
		else
		{
			pbarbr();
			echo "#$i: Answer has space chars.\n";
		}
	}

	if ($fout !== false)
	{
		$quizq [] = $qtrim;
		$quiza [] = $atrim;
		$quizi [] = $i;
	}
	$quizc ++;
}
pbarbr ();

fclose ($f);
unset ($f, $s, $q, $a, $strim, $qtrim, $atrim, $ar, $read);

echo "$quizc questions read. ", $i - $quizc, " skipped.\n";
if ($fout !== false)
	fwrite ($fout, "\n$quizc вопросов прочитано. ".($i-$quizc)." пропущено.\n");
else
{
	echo "Done.\n";
	exit (0);
}

echo "Memory usage for questionnaire: ", memory_get_usage () >> 10, " KBytes.\n";

/* -------------------- Поиск дублирующихся строк ----------------------------------------- */

if ($f_l) {
echo "Search for duplicated lines...\n";
fwrite ($fout, str_pad ("\nДублирующиеся строки: ", 100, '-') . "\n\n");

$qmd5 = array ();
for ($i=0; $i<$quizc; $i++)
{
	$s = md5 (uniform ($quizq [$i] . $quiza [$i]), true);
	if (isset ($qmd5 [$s]))
		$qmd5 [$s][] = $i;
	else	$qmd5 [$s] = array ($i);
}

$c = 0;
$cu = 0;
foreach ($qmd5 as $k => $v)
{
	if (count ($v) > 1)
	{
		$cu++;
		$c += count ($v);
		foreach ($v as $i)
			fwrite ($fout, "#${quizi[$i]}: ${quizq[$i]}|${quiza[$i]}\n");
		fwrite ($fout, "\n");
	}
}
if ($c != 0)
{
	fprintf ($fout, "Всего дублирующихся строк: ". ($c - $cu) ." (%.2f%%)\n", ($c - $cu) / $quizc * 100);
	fwrite ($fout, "Уникальных: $cu\n");
}
else	fwrite ($fout, "(нет таких)\n");
unset ($qmd5, $k, $v, $c, $cu);
} // if ($f_l)

/* -------------------- Поиск дублирующихся вопросов ----------------------------------------- */

if ($f_q) {
echo "Search for duplicated questions...\n";
fwrite ($fout, str_pad ("\nДублирующиеся вопросы: ", 100, '-') . "\n\n");

$qmd5 = array ();
for ($i=0; $i<$quizc; $i++)
{
	$s = md5 (uniform ($quizq [$i]), true);
	if (isset ($qmd5 [$s]))
		$qmd5 [$s][] = $i;
	else	$qmd5 [$s] = array ($i);
}

$c = 0;
$cu = 0;
foreach ($qmd5 as $k => $v)
{
	if (count ($v) > 1)
	{
		$cu++;
		$c += count ($v);
		foreach ($v as $i)
			fwrite ($fout, "#${quizi[$i]}: ${quizq[$i]}|${quiza[$i]}\n");
		fwrite ($fout, "\n");
	}
}
if ($c != 0)
{
	fprintf ($fout, "Всего дублирующихся вопросов: ". ($c - $cu) ." (%.2f%%)\n", ($c - $cu) / $quizc * 100);
	fwrite ($fout, "Уникальных: $cu\n");
} else
	fwrite ($fout, "(нет таких)\n");
unset ($qmd5, $k, $v, $c, $cu);
} // if ($f_q)

/* -------------------- Поиск дублирующихся ответов ----------------------------------------- */

if ($f_a) {
echo "Search for equal answers...\n";
fwrite ($fout, str_pad ("\nОдинаковые ответы: ", 100, '-') . "\n\n");

$answr = array ();
for ($i=0; $i<$quizc; $i++)
{
	$s = uniform ($quiza [$i]);
	if (isset ($answr [$s]))
		$answr [$s][] = $i;
	else	$answr [$s] = array ($i);
}

echo "Sorting equal answers...\n";
uasort ($answr, 'answr_cmp');

$c = 0;			// Счетчик вопросов с одинаковыми ответами
$qc = 0;		// Счетчик уникальных ответов
foreach ($answr as $k => $v)
{
	if (count ($v) > 1)
	{
		$qc++;
		$c += count ($v);
		foreach ($v as $i)
			fwrite ($fout, "#${quizi[$i]}: ${quizq[$i]}|${quiza[$i]}\n");
		fwrite ($fout, "\n");
	}
}
if ($c != 0)
{
	fprintf ($fout, "Всего одинаковых ответов: $qc (%.2f%%)\n", $qc / count ($answr) * 100);
	fprintf ($fout, "Всего вопросов с одинаковыми ответами: $c (%.2f%%)\n", $c / $quizc * 100);
} else
	fwrite ($fout, "(нет таких)\n");
echo "Peak memory usage: ", memory_get_usage () >> 10, " KBytes.\n";
unset ($answr, $k, $v, $qc, $c);
} // if ($f_a)

function answr_cmp ($a, $b)
{
	global $quiza;

	return strcasecmp ($quiza [$a[0]], $quiza [$b[0]]);
}

fclose ($fout);
echo "Done.\n";

function uniform ($s)		// преобразует одинаковые по написанию символы
				// управляется ключем -u
{
	global $f_u;

	if ($f_u)
		return strtr (strtolower ($s), "aeёkopcyx“”«»", "аеекорсух\"\"\"\"");
	return strtolower ($s);
}

$pbar_max = 0;
$pbar_prev = 0;
$pbar_outp = false;

function pbarinit ($max)
{
	global $pbar_max, $pbar_prev, $pbar_outp;

	$pbar_max = $max;
	$pbar_prev = 0;
	echo "0%";
	$pbar_outp = true;
}

function pbar ($cur)
{
	global $pbar_max, $pbar_prev, $pbar_outp;

	if ($pbar_max <= 0 || $pbar_prev >= $pbar_max)
	{
		$cur_percent = 10;
		if ($pbar_max <= 0 && $pbar_prev == 0)
			$prev_percent = 0;
		else	$prev_percent = 10;
	} else
	{
		$prev_percent = intval ($pbar_prev * 10 / $pbar_max);
		$cur_percent = intval ($cur * 10 / $pbar_max);
	}

	if ($cur_percent > $prev_percent)
	{
		for ($i=$prev_percent+1; $i<=$cur_percent; $i++)
			echo "....", $i * 10, "%";
		$pbar_outp = true;
	}
	if ($cur_percent >= 10)
		$pbar_prev = $pbar_max <= 0 ? 1 : $pbar_max;
	else	$pbar_prev = $cur;
}

function pbarbr ()
{
	global $pbar_outp;

	if ($pbar_outp)
	{
		echo "\n";
		$pbar_outp = false;
	}
}

?>