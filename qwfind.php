#!/bin/php
<?php

$f_win = strtoupper(substr(PHP_OS, 0, 3)) == 'WIN' ? true : false;

$words = array();
$quizfile = false;
for ($i=1; $i<$argc; $i++)
{
	if (substr ($argv[$i], 0, 1) == '-' || ($f_win ? substr ($argv[$i], 0, 1) == '/' : false))
	{
		$sc = substr ($argv[$i], 1);
		while ($sc != '')
		{
			switch (substr ($sc, 0, 1))
			{
			case 'f': if (strlen($sc)>1)
					$quizfile = substr($sc, 1);
				else
					$quizfile = $argv[++$i];
				goto cont;
			case 'h': show_help(); exit(0); break;
			default: echo "Warning: Unknown key '", substr($sc,0,1), "'\n";
			}
			$sc = substr ($sc, 1);
		}
cont:	continue;
	}
	array_push($words, cvt_con($argv[$i]));
}

if (count($words) < 1) {
	show_help();
	exit(0);
}

function show_help()
{
	global $argv;
	echo basename($argv[0]), " [-f qfile] words ..\n";
}

if ($quizfile == false) $quizfile = "php://stdin";

$f = fopen ($quizfile, 'rb');
if ($f === false)
{
	echo "Failed to open questions file '$quizfile'.\n";
	exit(1);
}

while (!feof($f))
{
	$s = fgets ($f);
	$s = rtrim ($s, "\n\r");
	$strim = trim ($s);
	if (strlen ($strim) == 0)		// Строка пуста
		continue;
	$ar = explode ('|', $s);
	if (count ($ar) < 2)			// Разделитель вопроса отсутствует
		continue;
	$q = $ar [0];
	$a = $ar [1];
	$qtrim = trim ($q);
	if (strlen ($qtrim) == 0)		// Строка вопроса пуста
		continue;
	$atrim = trim ($a);
	if (strlen ($atrim) == 0)		// Строка ответа пуста
		continue;
	foreach ($words as $word) {
		if (strcasecmp($atrim, $word) == 0)
			echo con_cvt($atrim), ": ", con_cvt($qtrim), "\n";
	}
}
fclose($f);

function con_cvt($s) {
	global $f_win;
	if ($f_win)	return convert_cyr_string($s, 'w', 'a');
	return mb_convert_encoding($s, 'UTF-8', 'CP1251');
}

function cvt_con($s) {
	global $f_win;
	if (!$f_win) return mb_convert_encoding($s, 'CP1251', 'UTF-8');
	return $s;
}

?>