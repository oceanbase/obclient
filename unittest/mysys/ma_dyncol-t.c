/* Copyright (c) 2011, Monty Program Ab
   Copyright (c) 2011, Oleksandr Byelkin

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
*/

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <ma_dyncol.h>
#include <tap.h>

void test_value_single_null()
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  /* init values */
  val.type= DYN_COL_NULL;
  mariadb_dyncol_value_init(&res);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= (res.type == DYN_COL_NULL);
err:
  ok(rc, "%s", "NULL");
  /* cleanup */
  mariadb_dyncol_free(&str);
}

void test_value_single_uint(ulonglong num, const char *name)
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  /* init values */
  val.type= DYN_COL_UINT;
  val.x.ulong_value= num;
  mariadb_dyncol_value_init(&res);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= (res.type == DYN_COL_UINT) && (res.x.ulong_value == num);
  num= res.x.ulong_value;
err:
  ok(rc, "%s - %llu", name, num);
  /* cleanup */
  mariadb_dyncol_free(&str);
}

void test_value_single_sint(longlong num, const char *name)
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  /* init values */
  val.type= DYN_COL_INT;
  val.x.long_value= num;
  mariadb_dyncol_value_init(&res);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= (res.type == DYN_COL_INT) && (res.x.long_value == num);
  num= res.x.ulong_value;
err:
  ok(rc, "%s - %lld", name, num);
  /* cleanup */
  mariadb_dyncol_free(&str);
}


void test_value_single_double(double num, const char *name)
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  /* init values */
  val.type= DYN_COL_DOUBLE;
  val.x.double_value= num;
  mariadb_dyncol_value_init(&res);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= (res.type == DYN_COL_DOUBLE) && (res.x.double_value == num);
  num= res.x.double_value;
err:
  ok(rc, "%s - %lf", name, num);
  /* cleanup */
  mariadb_dyncol_free(&str);
}

void test_value_single_decimal(const char *num)
{
  char *end= (((char*)num) + strlen(num));
  char buff[80] = {0};
  int rc= FALSE;
  int length= 80;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  memset(&str, 0, sizeof(str));

  /* init values */
  mariadb_dyncol_prepare_decimal(&val); // special procedure for decimal!!!
  if (string2decimal(num, &val.x.decimal.value, &end) != E_DEC_OK)
    goto err;
  mariadb_dyncol_value_init(&res);

  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= ((res.type == DYN_COL_DECIMAL) &&
       (decimal_cmp(&res.x.decimal.value, &val.x.decimal.value) == 0));
  decimal2string(&res.x.decimal.value, buff, &length, 0, 0, ' ');
err:
  ok(rc, "%s - %s", num, buff);
  /* cleanup */
  mariadb_dyncol_free(&str);
}

static CHARSET_INFO *charset_list[]=
{
#ifdef HAVE_CHARSET_big5
  &my_charset_big5_chinese_ci,
  &my_charset_big5_bin,
#endif
#ifdef HAVE_CHARSET_euckr
  &my_charset_euckr_korean_ci,
  &my_charset_euckr_bin,
#endif
#ifdef HAVE_CHARSET_gb2312
  &my_charset_gb2312_chinese_ci,
  &my_charset_gb2312_bin,
#endif
#ifdef HAVE_CHARSET_gbk
  &my_charset_gbk_chinese_ci,
  &my_charset_gbk_bin,
#endif
#ifdef HAVE_CHARSET_latin1
  &my_charset_latin1,
  &my_charset_latin1_bin,
#endif
#ifdef HAVE_CHARSET_sjis
  &my_charset_sjis_japanese_ci,
  &my_charset_sjis_bin,
#endif
#ifdef HAVE_CHARSET_tis620
  &my_charset_tis620_thai_ci,
  &my_charset_tis620_bin,
#endif
#ifdef HAVE_CHARSET_ujis
  &my_charset_ujis_japanese_ci,
  &my_charset_ujis_bin,
#endif
#ifdef HAVE_CHARSET_utf8
  &my_charset_utf8_general_ci,
#ifdef HAVE_UCA_COLLATIONS
  &my_charset_utf8_unicode_ci,
#endif
  &my_charset_utf8_bin,
#endif
};


void test_value_single_string(const char *string, size_t len,
                              CHARSET_INFO *cs)
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;

  /* init values */
  val.type= DYN_COL_STRING;
  val.x.string.value.str= (char*)string;
  val.x.string.value.length= len;
  val.x.string.charset= cs;
  mariadb_dyncol_value_init(&res);

  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= ((res.type == DYN_COL_STRING) &&
       (res.x.string.value.length == len) &&
       (memcmp(res.x.string.value.str, string, len) == 0) &&
       (res.x.string.charset->number == cs->number));
err:
  ok(rc, "'%s' - '%s' %u %u-%s", string,
     res.x.string.value.str, (uint)res.x.string.value.length,
     (uint)res.x.string.charset->number, res.x.string.charset->name);
  /* cleanup */
  val.x.string.value.str= NULL; // we did not allocated it
  mariadb_dyncol_free(&str);
}

void test_value_single_date(uint year, uint month, uint day, const char *name)
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  /* init values */
  val.type= DYN_COL_DATE;
  val.x.time_value.time_type= MYSQL_TIMESTAMP_DATE;
  val.x.time_value.year= year;
  val.x.time_value.month= month;
  val.x.time_value.day= day;
  mariadb_dyncol_value_init(&res);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= ((res.type == DYN_COL_DATE) &&
       (res.x.time_value.time_type == MYSQL_TIMESTAMP_DATE) &&
       (res.x.time_value.year == year) &&
       (res.x.time_value.month == month) &&
       (res.x.time_value.day == day));
err:
  ok(rc, "%s - %04u-%02u-%02u", name, year, month, day);
  /* cleanup */
  mariadb_dyncol_free(&str);
}

void test_value_single_time(uint neg, uint hour, uint minute, uint second,
                            uint mic, const char *name)
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  /* init values */
  val.type= DYN_COL_TIME;
  val.x.time_value.time_type= MYSQL_TIMESTAMP_TIME;
  val.x.time_value.neg= neg;
  val.x.time_value.hour= hour;
  val.x.time_value.minute= minute;
  val.x.time_value.second= second;
  val.x.time_value.second_part= mic;
  mariadb_dyncol_value_init(&res);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= ((res.type == DYN_COL_TIME) &&
       (res.x.time_value.time_type == MYSQL_TIMESTAMP_TIME) &&
       (res.x.time_value.neg == (int)neg) &&
       (res.x.time_value.hour == hour) &&
       (res.x.time_value.minute == minute) &&
       (res.x.time_value.second == second) &&
       (res.x.time_value.second_part == mic));
err:
  ok(rc, "%s - %c%02u:%02u:%02u.%06u", name, (neg ? '-' : '+'),
     hour, minute, second, mic);
  /* cleanup */
  mariadb_dyncol_free(&str);
}


void test_value_single_datetime(uint neg, uint year, uint month, uint day,
                                uint hour, uint minute, uint second,
                                uint mic, const char *name)
{
  int rc= FALSE;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  /* init values */
  val.type= DYN_COL_DATETIME;
  val.x.time_value.time_type= MYSQL_TIMESTAMP_DATETIME;
  val.x.time_value.neg= neg;
  val.x.time_value.year= year;
  val.x.time_value.month= month;
  val.x.time_value.day= day;
  val.x.time_value.hour= hour;
  val.x.time_value.minute= minute;
  val.x.time_value.second= second;
  val.x.time_value.second_part= mic;
  mariadb_dyncol_value_init(&res);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 1, ids, &val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  if (mariadb_dyncol_get_num(&str, 1, &res))
    goto err;
  rc= ((res.type == DYN_COL_DATETIME) &&
       (res.x.time_value.time_type == MYSQL_TIMESTAMP_DATETIME) &&
       (res.x.time_value.neg == (int)neg) &&
       (res.x.time_value.year == year) &&
       (res.x.time_value.month == month) &&
       (res.x.time_value.day == day) &&
       (res.x.time_value.hour == hour) &&
       (res.x.time_value.minute == minute) &&
       (res.x.time_value.second == second) &&
       (res.x.time_value.second_part == mic));
err:
  ok(rc, "%s - %c %04u-%02u-%02u %02u:%02u:%02u.%06u", name, (neg ? '-' : '+'),
     year, month, day, hour, minute, second, mic);
  /* cleanup */
  mariadb_dyncol_free(&str);
}


void test_value_multi(ulonglong num0,
                      longlong num1,
                      double num2,
                      const char *num3,
                      const char *string4, size_t len4, CHARSET_INFO *cs4,
                      uint year5, uint month5, uint day5,
                      uint neg6, uint hour6, uint minute6,
                      uint second6, uint mic6,
                      uint neg7, uint year7, uint month7, uint day7,
                      uint hour7, uint minute7, uint second7,
                      uint mic7,
                      uint *column_numbers,
                      const char *name)
{
  char *end3= (((char*)num3) + strlen(num3));
  int rc= FALSE;
  uint i;
  DYNAMIC_COLUMN_VALUE val[9], res[9];
  DYNAMIC_COLUMN str;
  
  memset(&str, 0, sizeof(str));
  /* init values */
  val[0].type= DYN_COL_UINT;
  val[0].x.ulong_value= num0;
  val[1].type= DYN_COL_INT;
  val[1].x.long_value= num1;
  val[2].type= DYN_COL_DOUBLE;
  val[2].x.double_value= num2;
  mariadb_dyncol_prepare_decimal(val + 3); // special procedure for decimal!!!
  if (string2decimal(num3, &val[3].x.decimal.value, &end3) != E_DEC_OK)
    goto err;
  val[4].type= DYN_COL_STRING;
  val[4].x.string.value.str= (char*)string4;
  val[4].x.string.value.length= len4;
  val[4].x.string.charset= cs4;
  val[5].type= DYN_COL_DATE;
  val[5].x.time_value.time_type= MYSQL_TIMESTAMP_DATE;
  val[5].x.time_value.year= year5;
  val[5].x.time_value.month= month5;
  val[5].x.time_value.day= day5;
  val[6].type= DYN_COL_TIME;
  val[6].x.time_value.time_type= MYSQL_TIMESTAMP_TIME;
  val[6].x.time_value.neg= neg6;
  val[6].x.time_value.hour= hour6;
  val[6].x.time_value.minute= minute6;
  val[6].x.time_value.second= second6;
  val[6].x.time_value.second_part= mic6;
  val[7].type= DYN_COL_DATETIME;
  val[7].x.time_value.time_type= MYSQL_TIMESTAMP_DATETIME;
  val[7].x.time_value.neg= neg7;
  val[7].x.time_value.year= year7;
  val[7].x.time_value.month= month7;
  val[7].x.time_value.day= day7;
  val[7].x.time_value.hour= hour7;
  val[7].x.time_value.minute= minute7;
  val[7].x.time_value.second= second7;
  val[7].x.time_value.second_part= mic7;
  val[8].type= DYN_COL_NULL;
  for (i= 0; i < 9; i++)
    mariadb_dyncol_value_init(res + i);
  /* create column */
  if (mariadb_dyncol_create_many_num(&str, 9, column_numbers, val, 1))
    goto err;
  dynstr_append(&str, "\1"); str.length--; //check for overflow
  /* read column */
  for (i= 0; i < 9; i++)
    if (mariadb_dyncol_get_num(&str, column_numbers[i], res + i))
      goto err;
  rc= ((res[0].type == DYN_COL_UINT) &&
       (res[0].x.ulong_value == num0) &&
       (res[1].type == DYN_COL_INT) &&
       (res[1].x.long_value == num1) &&
       (res[2].type == DYN_COL_DOUBLE) &&
       (res[2].x.double_value == num2) &&
       (res[3].type == DYN_COL_DECIMAL) &&
       (decimal_cmp(&res[3].x.decimal.value, &val[3].x.decimal.value) == 0) &&
       (res[4].type == DYN_COL_STRING) &&
       (res[4].x.string.value.length == len4) &&
       (memcmp(res[4].x.string.value.str, string4, len4) == 0) &&
       (res[4].x.string.charset->number == cs4->number) &&
       (res[5].type == DYN_COL_DATE) &&
       (res[5].x.time_value.time_type == MYSQL_TIMESTAMP_DATE) &&
       (res[5].x.time_value.year == year5) &&
       (res[5].x.time_value.month == month5) &&
       (res[5].x.time_value.day == day5) &&
       (res[6].type == DYN_COL_TIME) &&
       (res[6].x.time_value.time_type == MYSQL_TIMESTAMP_TIME) &&
       (res[6].x.time_value.neg == (int)neg6) &&
       (res[6].x.time_value.hour == hour6) &&
       (res[6].x.time_value.minute == minute6) &&
       (res[6].x.time_value.second == second6) &&
       (res[6].x.time_value.second_part == mic6) &&
       (res[7].type == DYN_COL_DATETIME) &&
       (res[7].x.time_value.time_type == MYSQL_TIMESTAMP_DATETIME) &&
       (res[7].x.time_value.neg == (int)neg7) &&
       (res[7].x.time_value.year == year7) &&
       (res[7].x.time_value.month == month7) &&
       (res[7].x.time_value.day == day7) &&
       (res[7].x.time_value.hour == hour7) &&
       (res[7].x.time_value.minute == minute7) &&
       (res[7].x.time_value.second == second7) &&
       (res[7].x.time_value.second_part == mic7) &&
       (res[8].type == DYN_COL_NULL));
err:
  ok(rc, "%s", name);
  /* cleanup */
  val[4].x.string.value.str= NULL; // we did not allocated it
  mariadb_dyncol_free(&str);
}


void test_value_multi_same_num()
{
  int rc= FALSE;
  uint i;
  DYNAMIC_COLUMN_VALUE val[5];
  uint column_numbers[]= {3,4,5,3,6}; // same column numbers
  DYNAMIC_COLUMN str;
  /* init values */
  for (i= 0; i < 5; i++)
    val[i].type= DYN_COL_NULL;
  /* create column */
  if (!mariadb_dyncol_create_many_num(&str, 5, column_numbers, val, 1))
    goto err;
  rc= TRUE;
err:
  ok(rc, "%s", "same column numbers check");
  /* cleanup */
  mariadb_dyncol_free(&str);
}


void test_update_multi(uint *column_numbers, uint *column_values,
                       my_bool *null_values, int only_add, int all)
{
  int rc= FALSE;
  int i, j;
  DYNAMIC_COLUMN str;
  DYNAMIC_COLUMN_VALUE val;

  val.type= DYN_COL_UINT;
  val.x.ulong_value= column_values[0];
  if (mariadb_dyncol_create_many_num(&str, 1, column_numbers, &val, 1))
    goto err;
  for (i= 1; i < all; i++)
  {
    val.type= (null_values[i] ? DYN_COL_NULL : DYN_COL_UINT);
    val.x.ulong_value= column_values[i];
    if (mariadb_dyncol_update_many_num(&str, 1, column_numbers +i, &val))
      goto err;

    /* check value(s) */
    for (j= i; j >= (i < only_add ? 0 : i); j--)
    {
      if (mariadb_dyncol_get_num(&str, column_numbers[j], &val))
        goto err;
      if (null_values[j])
      {
        if (val.type != DYN_COL_NULL ||
            mariadb_dyncol_exists_num(&str, column_numbers[j]) == ER_DYNCOL_YES)
          goto err;
      }
      else
      {
        if (val.type != DYN_COL_UINT ||
            val.x.ulong_value != column_values[j] ||
            mariadb_dyncol_exists_num(&str, column_numbers[j]) == ER_DYNCOL_NO)
          goto err;
      }
    }
    if (i < only_add)
    {
      uint elements, *num;
      if (mariadb_dyncol_list_num(&str, &elements, &num))
      {
        my_free(num);
        goto err;
      }
      /* cross check arrays */
      if ((int)elements != i + 1)
      {
        my_free(num);
        goto err;
      }
      for(j= 0; j < i + 1; j++)
      {
        int k;
        for(k= 0;
            k < i + 1 && column_numbers[j] != num[k];
            k++);
        if (k >= i + 1)
        {
          my_free(num);
          goto err;
        }
        for(k= 0;
            k < i + 1 && column_numbers[k] != num[j];
            k++);
        if (k >= i + 1)
        {
          my_free(num);
          goto err;
        }
      }
      my_free(num);
    }
  }

  rc= TRUE;
err:
  ok(rc, "%s", "add/delete/update");
  /* cleanup */
  mariadb_dyncol_free(&str);
}

void test_empty_string()
{
  DYNAMIC_COLUMN_VALUE val, res;
  DYNAMIC_COLUMN str;
  uint *array_of_uint;
  uint number_of_uint;
  int rc;
  uint ids[1]= {1};
  DYNAMIC_COLUMN_VALUE vals[1];
  /* empty string */
  bzero(&str, sizeof(str));

  rc= mariadb_dyncol_get_num(&str, 1, &res);
  ok( (rc == ER_DYNCOL_OK) && (res.type == DYN_COL_NULL), "%s", "empty get");

  vals[0].type= DYN_COL_NULL;
  rc= mariadb_dyncol_update_many_num(&str, 1, ids, vals);
  ok( (rc == ER_DYNCOL_OK) && (str.str == 0), "%s", "empty delete");

  rc= mariadb_dyncol_exists_num(&str, 1);
  ok( (rc == ER_DYNCOL_NO), "%s", "empty exists");

  rc= mariadb_dyncol_list_num(&str, &number_of_uint, &array_of_uint);
  ok( (rc == ER_DYNCOL_OK) && (number_of_uint == 0) && (str.str == 0),
      "%s", "empty list");

  val.type= DYN_COL_UINT;
  val.x.ulong_value= 1212;
  rc= mariadb_dyncol_update_many_num(&str, 1, ids, &val);
  if (rc == ER_DYNCOL_OK)
    rc= mariadb_dyncol_get_num(&str, 1, &res);
  ok( (rc == ER_DYNCOL_OK) && (str.str != 0) &&
      (res.type == DYN_COL_UINT) && (res.x.ulong_value == val.x.ulong_value),
      "%s", "empty update");
  mariadb_dyncol_free(&str);
}

static void test_mdev_4994()
{
  DYNAMIC_COLUMN dyncol;
  LEX_STRING key= {0,0};
  DYNAMIC_COLUMN_VALUE val;
  int rc;

  val.type= DYN_COL_NULL;

  mariadb_dyncol_init(&dyncol);
  rc= mariadb_dyncol_create_many_named(&dyncol, 1, &key, &val, 0);  /* crash */
  ok( (rc == ER_DYNCOL_OK), "%s", "test_mdev_4994");
  mariadb_dyncol_free(&dyncol);
}

static void test_mdev_4995()
{
  DYNAMIC_COLUMN dyncol;
  uint column_count= 5;
  int rc;

  mariadb_dyncol_init(&dyncol);
  rc= mariadb_dyncol_column_count(&dyncol,&column_count);

  ok( (rc == ER_DYNCOL_OK), "%s", "test_mdev_4995");
}

void test_update_many(uint *column_numbers, uint *column_values,
                      uint column_count,
                      uint *update_numbers, uint *update_values,
                      my_bool *update_nulls, uint update_count,
                      uint *result_numbers, uint *result_values,
                      uint result_count)
{
  int rc= FALSE;
  uint i;
  DYNAMIC_COLUMN str1;
  DYNAMIC_COLUMN str2;
  DYNAMIC_COLUMN_VALUE *val, *upd, *res;
  
  memset(&str1, 0, sizeof(str1));
  memset(&str2, 0, sizeof(str2));
  val = NULL;
  upd = NULL;
  res = NULL;

  val= (DYNAMIC_COLUMN_VALUE *)malloc(sizeof(DYNAMIC_COLUMN_VALUE) *
                                      column_count);
  upd= (DYNAMIC_COLUMN_VALUE *)malloc(sizeof(DYNAMIC_COLUMN_VALUE) *
                                      update_count);
  res= (DYNAMIC_COLUMN_VALUE *)malloc(sizeof(DYNAMIC_COLUMN_VALUE) *
                                      result_count);


  for (i= 0; i < column_count; i++)
  {
    val[i].type= DYN_COL_UINT;
    val[i].x.ulong_value= column_values[i];
  }
  for (i= 0; i < update_count; i++)
  {
    if (update_nulls[i])
      upd[i].type= DYN_COL_NULL;
    else
    {
      upd[i].type= DYN_COL_UINT;
      upd[i].x.ulong_value= update_values[i];
    }
  }
  for (i= 0; i < result_count; i++)
  {
    res[i].type= DYN_COL_UINT;
    res[i].x.ulong_value= result_values[i];
  }
  if (mariadb_dyncol_create_many_num(&str1, column_count, column_numbers, val, 1))
    goto err;
  if (mariadb_dyncol_update_many_num(&str1, update_count, update_numbers, upd))
    goto err;
  if (mariadb_dyncol_create_many_num(&str2, result_count, result_numbers, res, 1))
    goto err;
  if (str1.length == str2.length &&
      memcmp(str1.str, str2.str, str1.length) ==0)
    rc= TRUE;

err:
  ok(rc, "%s", "update_many");
  /* cleanup */
  free(val);
  free(upd);
  free(res);
  mariadb_dyncol_free(&str1);
  mariadb_dyncol_free(&str2);
}

static void test_mdev_9773()
{
  int rc;
  uint i;
  uint num_keys[5]= {1,2,3,4,5};
  char const *strval[]= {"Val1", "Val2", "Val3", "Val4", "Val5"};
  DYNAMIC_COLUMN_VALUE vals[5];
  DYNAMIC_COLUMN dynstr;
  uint unpack_columns= 0;
  MYSQL_LEX_STRING *unpack_keys= 0;
  DYNAMIC_COLUMN_VALUE *unpack_vals= 0;

  for (i = 0; i < 5; i++)
  {
    vals[i].type= DYN_COL_STRING;
    vals[i].x.string.value.str= (char *)strval[i];
    vals[i].x.string.value.length= strlen(strval[i]);
    vals[i].x.string.charset= &my_charset_latin1;
  }

  mariadb_dyncol_init(&dynstr);

  /* create numeric */
  rc= mariadb_dyncol_create_many_num(&dynstr, 5, num_keys, vals, 1);

  if (rc == ER_DYNCOL_OK)
    rc= mariadb_dyncol_unpack(&dynstr, &unpack_columns, &unpack_keys,
                              &unpack_vals);
  ok (rc == ER_DYNCOL_OK && unpack_columns == 5, "5 fields unpacked");
  for (i = 0; i < unpack_columns; i++)
  {
    ok(memcmp(unpack_vals[i].x.string.value.str,
               vals[i].x.string.value.str, vals[i].x.string.value.length) == 0,
       "unpack %u", i);
  }

  my_free(unpack_keys);
  my_free(unpack_vals);
  mariadb_dyncol_free(&dynstr);
}

int main(int argc __attribute__((unused)), char **argv)
{
  uint i;
  char *big_string= (char *)malloc(1024*1024);

  MY_INIT(argv[0]);
  plan(68);

  if (!big_string)
    exit(1);
  for (i= 0; i < 1024*1024; i++)
    big_string[i]= ('0' + (i % 10));
  test_value_single_null();
  test_value_single_uint(0, "0");
  test_value_single_uint(0xffffffffffffffffULL, "0xffffffffffffffff");
  test_value_single_uint(0xaaaaaaaaaaaaaaaaULL, "0xaaaaaaaaaaaaaaaa");
  test_value_single_uint(0x5555555555555555ULL, "0x5555555555555555");
  test_value_single_uint(27652, "27652");
  test_value_single_sint(0, "0");
  test_value_single_sint(1, "1");
  test_value_single_sint(-1, "-1");
  test_value_single_sint(0x7fffffffffffffffLL, "0x7fffffffffffffff");
  test_value_single_sint(0xaaaaaaaaaaaaaaaaLL, "0xaaaaaaaaaaaaaaaa");
  test_value_single_sint(0x5555555555555555LL, "0x5555555555555555");
  test_value_single_sint(0x8000000000000000LL, "0x8000000000000000");
  test_value_single_double(0.0, "0.0");
  test_value_single_double(1.0, "1.0");
  test_value_single_double(-1.0, "-1.0");
  test_value_single_double(1.0e100, "1.0e100");
  test_value_single_double(1.0e-100, "1.0e-100");
  test_value_single_double(9999999999999999999999999999999999999.0,
                           "9999999999999999999999999999999999999.0");
  test_value_single_double(-9999999999999999999999999999999999999.0,
                           "-9999999999999999999999999999999999999.0");
  test_value_single_decimal("0");
  test_value_single_decimal("1");
  test_value_single_decimal("-1");
  test_value_single_decimal("9999999999999999999999999999999");
  test_value_single_decimal("-9999999999999999999999999999999");
  test_value_single_decimal("0.9999999999999999999999999999999");
  test_value_single_decimal("-0.9999999999999999999999999999999");
  test_value_single_string("", 0, charset_list[0]);
  test_value_single_string("", 1, charset_list[0]);
  test_value_single_string("1234567890", 11, charset_list[0]);
  test_value_single_string("nulls\0\0\0\0\0", 10, charset_list[0]);
  sprintf(big_string, "%x", 0x7a);
  test_value_single_string(big_string, 0x7a, charset_list[0]);
  sprintf(big_string, "%x", 0x80);
  test_value_single_string(big_string, 0x80, charset_list[0]);
  sprintf(big_string, "%x", 0x7ffa);
  test_value_single_string(big_string, 0x7ffa, charset_list[0]);
  sprintf(big_string, "%x", 0x8000);
  test_value_single_string(big_string, 0x8000, charset_list[0]);
  sprintf(big_string, "%x", 1024*1024);
  test_value_single_string(big_string, 1024*1024, charset_list[0]);
  test_value_single_date(0, 0, 0, "zero date");
  test_value_single_date(9999, 12, 31, "max date");
  test_value_single_date(2011, 3, 26, "some date");
  test_value_single_time(0, 0, 0, 0, 0, "zero time");
  test_value_single_time(1, 23, 59, 59, 999999, "min time");
  test_value_single_time(0, 23, 59, 59, 999999, "max time");
  test_value_single_time(0, 21, 36, 20, 28, "some time");
  test_value_single_datetime(0, 0, 0, 0, 0, 0, 0, 0, "zero datetime");
  test_value_single_datetime(1, 9999, 12, 31, 23, 59, 59, 999999,
                             "min datetime");
  test_value_single_datetime(0, 9999, 12, 31, 23, 59, 59, 999999,
                             "max datetime");
  test_value_single_datetime(0, 2011, 3, 26, 21, 53, 12, 3445,
                             "some datetime");
  {
    uint column_numbers[]= {100,1,2,3,4,5,6,7,8};
    test_value_multi(0, 0, 0.0, "0",
                     "", 0, charset_list[0],
                     0, 0, 0,
                     0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0,
                     column_numbers,
                     "zero data");
  }
  {
    uint column_numbers[]= {10,1,12,37,4,57,6,76,87};
    test_value_multi(0xffffffffffffffffULL, 0x7fffffffffffffffLL,
                     99999999.999e120, "9999999999999999999999999999999",
                     big_string, 1024*1024, charset_list[0],
                     9999, 12, 31,
                     0, 23, 59, 59, 999999,
                     0, 9999, 12, 31, 23, 59, 59, 999999,
                     column_numbers,
                     "much data");
  }
  free(big_string);
  {
    uint column_numbers[]= {101,12,122,37,24,572,16,726,77};
    test_value_multi(37878, -3344,
                     2873.3874, "92743.238984789898",
                     "string", 6, charset_list[0],
                     2011, 3, 26,
                     1, 23, 23, 20, 333,
                     0, 2011, 3, 26, 23, 23, 53, 334,
                     column_numbers,
                     "zero data");
  }
  test_value_multi_same_num();
  {
    uint column_numbers[]= {1,2,3,4,5,6,7,2, 3, 4};
    uint column_values[]=  {1,2,3,4,5,6,7,0,30,40};
    my_bool null_values[]= {0,0,0,0,0,0,0,1, 0, 0};

    test_update_multi(column_numbers, column_values, null_values, 7, 10);
  }
  {
    uint column_numbers[]= {4,3,2,1, 1,2,3,4};
    uint column_values[]=  {4,3,2,1, 0,0,0,0};
    my_bool null_values[]= {0,0,0,0, 1,1,1,1};

    test_update_multi(column_numbers, column_values, null_values, 4, 8);
  }
  {
    uint column_numbers[]= {4,3,2,1, 4,3,2,1};
    uint column_values[]=  {4,3,2,1, 0,0,0,0};
    my_bool null_values[]= {0,0,0,0, 1,1,1,1};

    test_update_multi(column_numbers, column_values, null_values, 4, 8);
  }
  test_empty_string();
  {
    uint column_numbers[]= {1, 2, 3};
    uint column_values[]=  {1, 2, 3};
    uint update_numbers[]= {4, 3, 2, 1};
    uint update_values[]=  {40,30, 0,10};
    my_bool update_nulls[]={0, 0, 1, 0};
    uint result_numbers[]= {1, 3, 4};
    uint result_values[]=  {10,30,40};
    test_update_many(column_numbers, column_values, 3,
                     update_numbers, update_values, update_nulls, 4,
                     result_numbers, result_values, 3);
  }
  test_mdev_4994();
  test_mdev_4995();
  test_mdev_9773();

  my_end(0);
  return exit_status();
}
