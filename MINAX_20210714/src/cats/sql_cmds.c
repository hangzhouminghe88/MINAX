/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
/*
 *  This file contains all the SQL commands that are either issued by
 *   the Director or which are database backend specific.
 *
 *     Written by Kern Sibbald, July MMII
 */
/*
 * Note, PostgreSQL imposes some constraints on using DISTINCT and GROUP BY
 *  for example, the following is illegal in PostgreSQL:
 *  SELECT DISTINCT JobId FROM temp ORDER BY StartTime ASC;
 *  because all the ORDER BY expressions must appear in the SELECT list!
 */

#include "bacula.h"

const char *get_restore_objects = 
   "SELECT JobId,ObjectLength,ObjectFullLength,ObjectIndex,"
           "ObjectType,ObjectCompression,FileIndex,ObjectName,"
           "RestoreObject,PluginName "
    "FROM RestoreObject "
   "WHERE JobId IN (%s) "
     "AND ObjectType = %d "
   "ORDER BY ObjectIndex ASC";
const char *get_created_running_job =
   "SELECT JobId, Job, JobStatus FROM Job WHERE JobStatus IN ('C', 'R')";
const char *cleanup_created_job =
   "UPDATE Job SET JobStatus='f', StartTime=SchedTime, EndTime=SchedTime "
   "WHERE JobStatus = 'C'";
const char *cleanup_running_job = 
   "UPDATE Job SET JobStatus='f', EndTime=StartTime WHERE JobStatus = 'R'";

/* For sql_update.c db_update_stats */
const char *fill_jobhisto =
        "INSERT INTO JobHisto (JobId, Job, Name, Type, Level,"
           "ClientId, JobStatus,"
           "SchedTime, StartTime, EndTime, RealEndTime, JobTDate,"
           "VolSessionId, VolSessionTime, JobFiles, JobBytes, ReadBytes,"
           "JobErrors, JobMissingFiles, PoolId, FileSetId, PriorJobId, PriorJob, "
           "PurgedFiles, HasBase, Reviewed, Comment)"
        "SELECT JobId, Job, Name, Type, Level, ClientId, JobStatus,"
           "SchedTime, StartTime, EndTime, RealEndTime, JobTDate,"
           "VolSessionId, VolSessionTime, JobFiles, JobBytes, ReadBytes,"
           "JobErrors, JobMissingFiles, PoolId, FileSetId, PriorJobId, PriorJob, "
           "PurgedFiles, HasBase, Reviewed, Comment "
          "FROM Job "
         "WHERE JobStatus IN ('T','W','f','A','E')"
           "AND NOT EXISTS "
                "(SELECT JobHisto.JobId "
                   "FROM JobHisto WHERE JobHisto.Jobid=Job.JobId)"
           "AND JobTDate < %s ";

/* For ua_update.c */
const char *list_pool = "SELECT * FROM Pool WHERE PoolId=%s";

/* For ua_dotcmds.c */
const char *client_backups =
   "SELECT DISTINCT Job.JobId,Client.Name as Client,Level,StartTime,"
   "JobFiles,JobBytes,VolumeName,MediaType,FileSet,Media.Enabled as Enabled"
   " FROM Client,Job,JobMedia,Media,FileSet"
   " WHERE Client.Name='%s'"
   " AND FileSet='%s'"
   " AND Client.ClientId=Job.ClientId "
   " AND JobStatus IN ('T','W') AND Type='B' "
   " AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId "
   " AND Job.FileSetId=FileSet.FileSetId"
   " ORDER BY Job.StartTime";

/* ====== ua_prune.c */

const char *sel_JobMedia = 
   "SELECT DISTINCT JobMedia.JobId FROM JobMedia,Job"
   " WHERE MediaId=%s AND Job.JobId=JobMedia.JobId "
   " AND Job.JobTDate<%s AND Job.JobStatus NOT IN ('R', 'C') ";

/* Delete temp tables and indexes  */
const char *drop_deltabs[] = {
   "DROP TABLE IF EXISTS DelCandidates",
   NULL};

const char *create_delindex = "CREATE INDEX DelInx1 ON DelCandidates (JobId)";

/* ======= ua_restore.c */
const char *uar_count_files =
   "SELECT JobFiles FROM Job WHERE JobId=%s";

/* List last 20 Jobs */
const char *uar_list_jobs =
   "SELECT JobId,Client.Name as Client,Job.Name as Name,StartTime,Level as "
   "JobLevel,JobFiles,JobBytes "
   "FROM Client,Job WHERE Client.ClientId=Job.ClientId AND JobStatus IN ('T','W') "
   "AND Type='B' ORDER BY StartTime DESC LIMIT 20";

const char *uar_print_jobs = 
   "SELECT DISTINCT JobId,Level,JobFiles,JobBytes,StartTime,VolumeName"
   " FROM Job JOIN JobMedia USING (JobId) JOIN Media USING (MediaId) "
   " WHERE JobId IN (%s) "
   " ORDER BY StartTime ASC";

/*
 * Find all files for a particular JobId and insert them into
 *  the tree during a restore.
 */
const char *uar_sel_files =
   "SELECT Path.Path,File.Filename,FileIndex,JobId,LStat "
     "FROM File JOIN Path USING (PathId) "
    "WHERE File.JobId IN (%s)";

const char *uar_del_temp  = "DROP TABLE IF EXISTS temp";
const char *uar_del_temp1 = "DROP TABLE IF EXISTS temp1";

const char *uar_last_full =
   "INSERT INTO temp1(JobId,JobTDate) SELECT Job.JobId,JobTdate "
   "FROM Client,Job,JobMedia,Media,FileSet WHERE Client.ClientId=%s "
   "AND Job.ClientId=%s "
   "AND Job.StartTime < '%s' "
   "AND Level='F' AND JobStatus IN ('T','W') AND Type='B' "
   "AND JobMedia.JobId=Job.JobId "
   "AND Media.Enabled=1 "
   "AND JobMedia.MediaId=Media.MediaId "
   "AND Job.FileSetId=FileSet.FileSetId "
   "AND FileSet.FileSet='%s' "
   "%s"
   "ORDER BY Job.JobTDate DESC LIMIT 1";

const char *uar_full =
   "INSERT INTO temp (JobId,JobTDate,ClientId,Level,JobFiles,JobBytes,StartTime,VolumeName,StartFile,VolSessionId,VolSessionTime) "

   "SELECT Job.JobId,Job.JobTDate,"
   "Job.ClientId,Job.Level,Job.JobFiles,Job.JobBytes,"
   "StartTime,VolumeName,JobMedia.StartFile,VolSessionId,VolSessionTime "
   "FROM temp1,Job,JobMedia,Media WHERE temp1.JobId=Job.JobId "
   "AND Level='F' AND JobStatus IN ('T','W') AND Type='B' "
   "AND Media.Enabled=1 "
   "AND JobMedia.JobId=Job.JobId "
   "AND JobMedia.MediaId=Media.MediaId";

const char *uar_dif =
   "INSERT INTO temp (JobId,JobTDate,ClientId,Level,JobFiles,JobBytes,StartTime,VolumeName,StartFile,VolSessionId,VolSessionTime) "
   "SELECT Job.JobId,Job.JobTDate,Job.ClientId,"
   "Job.Level,Job.JobFiles,Job.JobBytes,"
   "Job.StartTime,Media.VolumeName,JobMedia.StartFile,"
   "Job.VolSessionId,Job.VolSessionTime "
   "FROM Job,JobMedia,Media,FileSet "
   "WHERE Job.JobTDate>%s AND Job.StartTime<'%s' "
   "AND Job.ClientId=%s "
   "AND JobMedia.JobId=Job.JobId "
   "AND Media.Enabled=1 "
   "AND JobMedia.MediaId=Media.MediaId "
   "AND Job.Level='D' AND JobStatus IN ('T','W') AND Type='B' "
   "AND Job.FileSetId=FileSet.FileSetId "
   "AND FileSet.FileSet='%s' "
   "%s"
   "ORDER BY Job.JobTDate DESC LIMIT 1";

const char *uar_inc =
   "INSERT INTO temp (JobId,JobTDate,ClientId,Level,JobFiles,JobBytes,StartTime,VolumeName,StartFile,VolSessionId,VolSessionTime) "
   "SELECT Job.JobId,Job.JobTDate,Job.ClientId,"
   "Job.Level,Job.JobFiles,Job.JobBytes,"
   "Job.StartTime,Media.VolumeName,JobMedia.StartFile,"
   "Job.VolSessionId,Job.VolSessionTime "
   "FROM Job,JobMedia,Media,FileSet "
   "WHERE Job.JobTDate>%s AND Job.StartTime<'%s' "
   "AND Job.ClientId=%s "
   "AND Media.Enabled=1 "
   "AND JobMedia.JobId=Job.JobId "
   "AND JobMedia.MediaId=Media.MediaId "
   "AND Job.Level='I' AND JobStatus IN ('T','W') AND Type='B' "
   "AND Job.FileSetId=FileSet.FileSetId "
   "AND FileSet.FileSet='%s' "
   "%s";

const char *uar_list_temp =
   "SELECT DISTINCT JobId,Level,JobFiles,JobBytes,StartTime,VolumeName"
   " FROM temp"
   " ORDER BY StartTime ASC";


const char *uar_sel_jobid_temp = 
   "SELECT DISTINCT JobId,StartTime FROM temp ORDER BY StartTime ASC";

const char *uar_sel_all_temp1 = "SELECT * FROM temp1";

const char *uar_sel_all_temp = "SELECT * FROM temp";



/* Select FileSet names for this Client */
const char *uar_sel_fileset =
   "SELECT DISTINCT FileSet.FileSet FROM Job,"
   "Client,FileSet WHERE Job.FileSetId=FileSet.FileSetId "
   "AND Job.ClientId=%s AND Client.ClientId=%s "
   "ORDER BY FileSet.FileSet";

/* Select all different FileSet for this client
 * This query doesn't guarantee that the Id is the latest
 * version of the FileSet. Can be used with other queries that
 * use Ids to select the FileSet name. (like in accurate)
 */
const char *uar_sel_filesetid =
   "SELECT MAX(FileSet.FileSetId) "
     "FROM FileSet JOIN Job USING (FileSetId) "
         "WHERE Job.ClientId=%s "
        "GROUP BY FileSet";

/*
 *  Find JobId, FileIndex for a given path/file and date
 *  for use when inserting individual files into the tree.
 */
const char *uar_jobid_fileindex =
   "SELECT Job.JobId,File.FileIndex FROM Job,File,Path,Client "
   "WHERE Job.JobId=File.JobId "
   "AND Job.StartTime<='%s' "
   "AND Path.Path='%s' "
   "AND File.Filename='%s' "
   "AND Client.Name='%s' "
   "AND Job.ClientId=Client.ClientId "
   "AND Path.PathId=File.PathId "
   "AND JobStatus IN ('T','W') AND Type='B' "
   "ORDER BY Job.StartTime DESC LIMIT 1";

const char *uar_jobids_fileindex =
   "SELECT Job.JobId,File.FileIndex FROM Job,File,Path,Client "
   "WHERE Job.JobId IN (%s) "
   "AND Job.JobId=File.JobId "
   "AND Job.StartTime<='%s' "
   "AND Path.Path='%s' "
   "AND File.Filename='%s' "
   "AND Client.Name='%s' "
   "AND Job.ClientId=Client.ClientId "
   "AND Path.PathId=File.PathId "
   "ORDER BY Job.StartTime DESC LIMIT 1";

/* Query to get list of files from table -- presuably built by an external program */
const char *uar_jobid_fileindex_from_table = 
   "SELECT JobId, FileIndex FROM %s ORDER BY JobId, FileIndex ASC";

/* Get the list of the last recent version per Delta with a given
 *  jobid list. This is a tricky part because with SQL the result of:
 *
 *   SELECT MAX(A), B, C, D FROM... GROUP BY (B,C)
 *
 * doesn't give the good result (for D).
 *
 * With PostgreSQL, we can use DISTINCT ON(), but with Mysql or Sqlite,
 *  we need an extra join using JobTDate.
 */
static const char *select_recent_version_with_basejob_default = 
"SELECT FileId, Job.JobId AS JobId, FileIndex, File.PathId AS PathId,"
       "File.Filename AS Filename, LStat, MD5, DeltaSeq,"
       "Job.JobTDate AS JobTDate "
"FROM Job, File, ( "
    "SELECT MAX(JobTDate) AS JobTDate, PathId, Filename "
      "FROM ( "
        "SELECT JobTDate, PathId, Filename "   /* Get all normal files */
          "FROM File JOIN Job USING (JobId) "    /* from selected backup */
         "WHERE File.JobId IN (%s) "
          "UNION ALL "
        "SELECT JobTDate, PathId, Filename "   /* Get all files from */
          "FROM BaseFiles "                      /* BaseJob */
               "JOIN File USING (FileId) "
               "JOIN Job  ON    (BaseJobId = Job.JobId) "
         "WHERE BaseFiles.JobId IN (%s) "        /* Use Max(JobTDate) to find */
       ") AS tmp "
       "GROUP BY PathId, Filename "            /* the latest file version */
    ") AS T1 "
"WHERE (Job.JobId IN ( "  /* Security, we force JobId to be valid */
        "SELECT DISTINCT BaseJobId FROM BaseFiles WHERE JobId IN (%s)) "
        "OR Job.JobId IN (%s)) "
  "AND T1.JobTDate = Job.JobTDate " /* Join on JobTDate to get the orginal */
  "AND Job.JobId = File.JobId "     /* Job/File record */
  "AND T1.PathId = File.PathId "
  "AND T1.Filename = File.Filename";

const char *select_recent_version_with_basejob[] =
{
   /* MySQL  */
   select_recent_version_with_basejob_default,

   /* PostgreSQL */   /* The DISTINCT ON () permits to avoid extra join */
   "SELECT DISTINCT ON (Filename, PathId) JobTDate, JobId, FileId, "
         "FileIndex, PathId, Filename, LStat, MD5, DeltaSeq "
   "FROM "
     "(SELECT FileId, JobId, PathId, Filename, FileIndex, LStat, MD5, DeltaSeq "
         "FROM File WHERE JobId IN (%s) "
        "UNION ALL "
       "SELECT File.FileId, File.JobId, PathId, Filename, "
              "File.FileIndex, LStat, MD5, DeltaSeq "
         "FROM BaseFiles JOIN File USING (FileId) "
        "WHERE BaseFiles.JobId IN (%s) "
       ") AS T JOIN Job USING (JobId) "
   "ORDER BY Filename, PathId, JobTDate DESC ",

   /* SQLite */
   select_recent_version_with_basejob_default
};
 
/* We do the same thing than the previous query, but we include
 * all delta parts. If the file has been deleted, we can have irrelevant
 * parts.
 *
 * The code that uses results should control the delta sequence with
 * the following rules:
 * First Delta = 0
 * Delta = Previous Delta + 1
 *
 * If we detect a gap, we can discard further pieces
 * If a file starts at 1 instead of 0, the file has been deleted, and further
 *   pieces are useless.
 * This control should be reset for each new file
 */
static const char *select_recent_version_with_basejob_and_delta_default = 
"SELECT FileId, Job.JobId AS JobId, FileIndex, File.PathId AS PathId,"
       "File.Filename AS Filename, LStat, MD5, File.DeltaSeq AS DeltaSeq,"
       "Job.JobTDate AS JobTDate"
" FROM Job, File, ("
    "SELECT MAX(JobTDate) AS JobTDate, PathId, Filename, DeltaSeq "
      "FROM ("
       "SELECT JobTDate, PathId, Filename, DeltaSeq " /*Get all normal files*/
         "FROM File JOIN Job USING (JobId) "          /* from selected backup */
        "WHERE File.JobId IN (%s) "
         "UNION ALL "
       "SELECT JobTDate, PathId, Filename, DeltaSeq " /*Get all files from */
         "FROM BaseFiles "                            /* BaseJob */
              "JOIN File USING (FileId) "
              "JOIN Job  ON    (BaseJobId = Job.JobId) "
        "WHERE BaseFiles.JobId IN (%s) "        /* Use Max(JobTDate) to find */
       ") AS tmp "
       "GROUP BY PathId, Filename, DeltaSeq "    /* the latest file version */
    ") AS T1"
" WHERE (Job.JobId IN ( "  /* Security, we force JobId to be valid */
        "SELECT DISTINCT BaseJobId FROM BaseFiles WHERE JobId IN (%s))"
        " OR Job.JobId IN (%s))"
  " AND T1.JobTDate = Job.JobTDate" /* Join on JobTDate to get the orginal */
  " AND Job.JobId = File.JobId"     /* Job/File record */
  " AND T1.PathId = File.PathId"
  " AND T1.Filename = File.Filename";

const char *select_recent_version_with_basejob_and_delta[] =
{
   /* MySQL  */
   select_recent_version_with_basejob_and_delta_default,

   /* Postgresql -- The DISTINCT ON () permits to avoid extra join */
   "SELECT DISTINCT ON (Filename, PathId, DeltaSeq) JobTDate, JobId, FileId, "
         "FileIndex, PathId, Filename, LStat, MD5, DeltaSeq "
   "FROM "
    "(SELECT FileId, JobId, PathId, Filename, FileIndex, LStat, MD5,DeltaSeq "
         "FROM File WHERE JobId IN (%s) "
        "UNION ALL "
       "SELECT File.FileId, File.JobId, PathId, Filename, "
              "File.FileIndex, LStat, MD5, DeltaSeq "
         "FROM BaseFiles JOIN File USING (FileId) "
        "WHERE BaseFiles.JobId IN (%s) "
       ") AS T JOIN Job USING (JobId) "
   "ORDER BY Filename, PathId, DeltaSeq, JobTDate DESC ",

   /* SQLite */
   select_recent_version_with_basejob_and_delta_default
};

/* Get the list of the last recent version with a given BaseJob jobid list
 * We don't handle Delta with BaseJobs, they have only Full files
 */
static const char *select_recent_version_default = 
  "SELECT j1.JobId AS JobId, f1.FileId AS FileId, f1.FileIndex AS FileIndex, "
          "f1.PathId AS PathId, f1.Filename AS Filename, "
          "f1.LStat AS LStat, f1.MD5 AS MD5, j1.JobTDate "
     "FROM ( "     /* Choose the last version for each Path/Filename */
       "SELECT max(JobTDate) AS JobTDate, PathId, Filename "
         "FROM File JOIN Job USING (JobId) "
        "WHERE File.JobId IN (%s) "
       "GROUP BY PathId, Filename "
     ") AS t1, Job AS j1, File AS f1 "
    "WHERE t1.JobTDate = j1.JobTDate "
      "AND j1.JobId IN (%s) "
      "AND t1.Filename = f1.Filename "
      "AND t1.PathId = f1.PathId "
      "AND j1.JobId = f1.JobId";

const char *select_recent_version[] =
{
   /* MySQL */
   select_recent_version_default,

   /* PostgreSQL */
   "SELECT DISTINCT ON (Filename, PathId) JobTDate, JobId, FileId, "
          "FileIndex, PathId, Filename, LStat, MD5 "
     "FROM File JOIN Job USING (JobId) "
    "WHERE JobId IN (%s) "
    "ORDER BY Filename, PathId, JobTDate DESC ",

   /* SQLite */
   select_recent_version_default
};

/* We don't create this table as TEMPORARY because MySQL 
    MyISAM 5.0 and 5.1 are unable to run further queries in this mode
 */
static const char *create_temp_accurate_jobids_default =
    "CREATE TABLE btemp3%s AS "
    "SELECT JobId, StartTime, EndTime, JobTDate, PurgedFiles, FileSetId "
      "FROM Job JOIN FileSet USING (FileSetId) "
     "WHERE ClientId = %s "
       "AND Level='F' AND JobStatus IN ('T','W') AND Type='B' "
       "AND StartTime<'%s' "
       "AND FileSet.FileSet=(SELECT FileSet FROM FileSet WHERE FileSetId = %s) "
       " %s "                   /* Any filter */
     "ORDER BY Job.JobTDate DESC LIMIT 1";

static const char *create_temp_accurate_jobids_mysql =
    "CREATE TABLE btemp3%s /*PKEY (DummyPkey INTEGER AUTO_INCREMENT PRIMARY KEY)*/ AS ("
    "SELECT JobId, StartTime, EndTime, JobTDate, PurgedFiles, FileSetId "
      "FROM Job JOIN FileSet USING (FileSetId) "
     "WHERE ClientId = %s "
       "AND Level='F' AND JobStatus IN ('T','W') AND Type='B' "
       "AND StartTime<'%s' "
       "AND FileSet.FileSet=(SELECT FileSet FROM FileSet WHERE FileSetId = %s) "
       " %s "                   /* Any filter */
     "ORDER BY Job.JobTDate DESC LIMIT 1)";

const char *create_temp_accurate_jobids[] =
{
   /* MySQL */
   create_temp_accurate_jobids_mysql,
   /* PostgreSQL */
   create_temp_accurate_jobids_default,
   /* SQLite */
   create_temp_accurate_jobids_default
};
 
const char *create_temp_basefile[] =
{
   /* MySQL */
   "CREATE TEMPORARY TABLE basefile%lld"
   "(Path BLOB NOT NULL, Name BLOB NOT NULL, "
   "INDEX (Path(255), Name(255))"
   "/*PKEY, DummyPkey INTEGER AUTO_INCREMENT PRIMARY KEY*/)",
   /* PostgreSQL */
   "CREATE TEMPORARY TABLE basefile%lld"
   "(Path TEXT, Name TEXT)",
   /* SQLite */
   "CREATE TEMPORARY TABLE basefile%lld"
   "(Path TEXT, Name TEXT)"
}; 
 
const char *create_temp_new_basefile[] =
{
   /* MySQL */
   "CREATE TEMPORARY TABLE new_basefile%lld /*PKEY(DummyPkey INTEGER AUTO_INCREMENT PRIMARY KEY)*/ AS "
   "SELECT Path.Path AS Path, Temp.Filename AS Name, Temp.FileIndex AS FileIndex,"
   " Temp.JobId AS JobId, Temp.LStat AS LStat, Temp.FileId AS FileId,"
   " Temp.MD5 AS MD5"
   " FROM (%s) AS Temp"
   " JOIN Path ON (Path.PathId = Temp.PathId)"
   " WHERE Temp.FileIndex > 0",
 
   /* PostgreSQL */
   "CREATE TEMPORARY TABLE new_basefile%lld AS  "
   "SELECT Path.Path AS Path, Temp.Filename AS Name, Temp.FileIndex AS FileIndex,"
   " Temp.JobId AS JobId, Temp.LStat AS LStat, Temp.FileId AS FileId,"
   " Temp.MD5 AS MD5"
   " FROM ( %s ) AS Temp"
   " JOIN Path ON (Path.PathId = Temp.PathId)"
   " WHERE Temp.FileIndex > 0",
 
   /* SQLite */
   "CREATE TEMPORARY TABLE new_basefile%lld AS  "
   "SELECT Path.Path AS Path, Temp.Filename AS Name, Temp.FileIndex AS FileIndex,"
   " Temp.JobId AS JobId, Temp.LStat AS LStat, Temp.FileId AS FileId,"
   " Temp.MD5 AS MD5"
   " FROM ( %s ) AS Temp"
   " JOIN Path ON (Path.PathId = Temp.PathId)"
   " WHERE Temp.FileIndex > 0"
}; 
 
/* ====== ua_prune.c ====== */

/* List of SQL commands to create temp table and indicies  */
const char *create_deltabs[] =
{
   /* MySQL */
   "CREATE TEMPORARY TABLE DelCandidates ("
   "JobId INTEGER UNSIGNED NOT NULL, "
   "PurgedFiles TINYINT, "
   "FileSetId INTEGER UNSIGNED, "
   "JobFiles INTEGER UNSIGNED, "
   "JobStatus BINARY(1)"
   "/*PKEY, DummyPkey INTEGER AUTO_INCREMENT PRIMARY KEY*/)",
 
   /* PostgreSQL */
   "CREATE TEMPORARY TABLE DelCandidates ( "
   "JobId INTEGER NOT NULL, "
   "PurgedFiles SMALLINT, "
   "FileSetId INTEGER, "
   "JobFiles INTEGER, "
   "JobStatus char(1))",
 
   /* SQLite */
   "CREATE TEMPORARY TABLE DelCandidates ("
   "JobId INTEGER UNSIGNED NOT NULL, "
   "PurgedFiles TINYINT, "
   "FileSetId INTEGER UNSIGNED, "
   "JobFiles INTEGER UNSIGNED, "
   "JobStatus CHAR)"
}; 

/* ======= ua_purge.c ====== */
/* Select the first available Copy Job that must be upgraded 
 *   to a Backup job when the original backup job is expired.
 */
static const char *uap_upgrade_copies_oldest_job_default = 
    "CREATE TEMPORARY TABLE cpy_tmp /*PKEY (DummyPkey INTEGER PRIMARY KEY DEFAULT 1)*/ AS "
       "SELECT MIN(JobId) AS JobId FROM Job "     /* Choose the oldest job */
        "WHERE Type='%c' "                        /* JT_JOB_COPY */
          "AND ( PriorJobId IN (%s) "             /* JobId selection */
              "OR "
               " PriorJobId IN ( "
                  "SELECT PriorJobId "
                    "FROM Job "
                   "WHERE JobId IN (%s) "         /* JobId selection */
                    " AND Type='B' AND JobStatus IN ('T', 'W') "
                 ") "
              ") "
          "GROUP BY PriorJobId ";           /* one result per copy */

const char *uap_upgrade_copies_oldest_job[] =
{
   /* MySQL */
   uap_upgrade_copies_oldest_job_default,
   /* PostgreSQL */
   uap_upgrade_copies_oldest_job_default,
   /* SQLite */
   uap_upgrade_copies_oldest_job_default
}; 
 
/* ======= ua_restore.c ====== */

/* List Jobs where a particular file is saved */
const char *uar_file[] =
{
   /* MySQL */
   "SELECT Job.JobId as JobId,"
   "CONCAT(Path.Path,File.Filename) as Name, "
   "StartTime,Type as JobType,JobStatus,JobFiles,JobBytes "
   "FROM Client,Job,File,Path WHERE Client.Name='%s' "
   "AND Client.ClientId=Job.ClientId "
   "AND Job.JobId=File.JobId AND File.FileIndex > 0 "
   "AND Path.PathId=File.PathId "
   "AND File.Filename='%s' ORDER BY StartTime DESC LIMIT 20",
 
   /* PostgreSQL */
   "SELECT Job.JobId as JobId,"
   "Path.Path||File.Filename as Name, "
   "StartTime,Type as JobType,JobStatus,JobFiles,JobBytes "
   "FROM Client,Job,File,Path WHERE Client.Name='%s' "
   "AND Client.ClientId=Job.ClientId "
   "AND Job.JobId=File.JobId AND File.FileIndex > 0 "
   "AND Path.PathId=File.PathId "
   "AND File.Filename='%s' ORDER BY StartTime DESC LIMIT 20",
 
   /* SQLite */
   "SELECT Job.JobId as JobId,"
   "Path.Path||File.Filename as Name, "
   "StartTime,Type as JobType,JobStatus,JobFiles,JobBytes "
   "FROM Client,Job,File,Path WHERE Client.Name='%s' "
   "AND Client.ClientId=Job.ClientId "
   "AND Job.JobId=File.JobId AND File.FileIndex > 0 "
   "AND Path.PathId=File.PathId "
   "AND File.Filename='%s' ORDER BY StartTime DESC LIMIT 20"
}; 
 
const char *uar_create_temp[] =
{
   /* MySQL */
   "CREATE TEMPORARY TABLE temp ("
   "JobId INTEGER UNSIGNED NOT NULL,"
   "JobTDate BIGINT UNSIGNED,"
   "ClientId INTEGER UNSIGNED,"
   "Level CHAR,"
   "JobFiles INTEGER UNSIGNED,"
   "JobBytes BIGINT UNSIGNED,"
   "StartTime TEXT,"
   "VolumeName TEXT,"
   "StartFile INTEGER UNSIGNED,"
   "VolSessionId INTEGER UNSIGNED,"
   "VolSessionTime INTEGER UNSIGNED"
   "/*PKEY, DummyPKey INTEGER AUTO_INCREMENT PRIMARY KEY*/)",
 
   /* PostgreSQL */
   "CREATE TEMPORARY TABLE temp ("
   "JobId INTEGER NOT NULL,"
   "JobTDate BIGINT,"
   "ClientId INTEGER,"
   "Level CHAR,"
   "JobFiles INTEGER,"
   "JobBytes BIGINT,"
   "StartTime TEXT,"
   "VolumeName TEXT,"
   "StartFile INTEGER,"
   "VolSessionId INTEGER,"
   "VolSessionTime INTEGER)",
 
   /* SQLite */
   "CREATE TEMPORARY TABLE temp ("
   "JobId INTEGER UNSIGNED NOT NULL,"
   "JobTDate BIGINT UNSIGNED,"
   "ClientId INTEGER UNSIGNED,"
   "Level CHAR,"
   "JobFiles INTEGER UNSIGNED,"
   "JobBytes BIGINT UNSIGNED,"
   "StartTime TEXT,"
   "VolumeName TEXT,"
   "StartFile INTEGER UNSIGNED,"
   "VolSessionId INTEGER UNSIGNED,"
   "VolSessionTime INTEGER UNSIGNED)"
}; 

const char *uar_create_temp1[] =
{
   /* MySQL */
   "CREATE TEMPORARY TABLE temp1 ("
   "JobId INTEGER UNSIGNED NOT NULL,"
   "JobTDate BIGINT UNSIGNED"
   "/*PKEY, DummyPKey INTEGER AUTO_INCREMENT PRIMARY KEY*/)",
   /* PostgreSQL */
   "CREATE TEMPORARY TABLE temp1 ("
   "JobId INTEGER NOT NULL,"
   "JobTDate BIGINT)",
   /* SQLite */
   "CREATE TEMPORARY TABLE temp1 ("
   "JobId INTEGER UNSIGNED NOT NULL,"
   "JobTDate BIGINT UNSIGNED)"
}; 

/* Query to get all files in a directory no recursing
 *  Note, for PostgreSQL since it respects the "Single Value
 *  rule", the results of the SELECT will be unoptimized.
 *  I.e. the same file will be restored multiple times, once
 *  for each time it was backed up.
 */

const char *uar_jobid_fileindex_from_dir[] =
{
   /* MySQL */
   "SELECT Job.JobId,File.FileIndex FROM Job,File,Path,Client "
   "WHERE Job.JobId IN (%s) "
   "AND Job.JobId=File.JobId "
   "AND Path.Path='%s' "
   "AND Client.Name='%s' "
   "AND Job.ClientId=Client.ClientId "
   "AND Path.PathId=File.Pathid "
   "GROUP BY File.FileIndex ORDER BY Job.StartTime",
 
   /* PostgreSQL */
   "SELECT Job.JobId,File.FileIndex FROM Job,File,Path,Client "
   "WHERE Job.JobId IN (%s) "
   "AND Job.JobId=File.JobId "
   "AND Path.Path='%s' "
   "AND Client.Name='%s' "
   "AND Job.ClientId=Client.ClientId "
   "AND Path.PathId=File.Pathid ORDER BY Job.StartTime ",
 
   /* SQLite */
   "SELECT Job.JobId,File.FileIndex FROM Job,File,Path,Client "
   "WHERE Job.JobId IN (%s) "
   "AND Job.JobId=File.JobId "
   "AND Path.Path='%s' "
   "AND Client.Name='%s' "
   "AND Job.ClientId=Client.ClientId "
   "AND Path.PathId=File.Pathid "
   "GROUP BY File.FileIndex ORDER BY Job.StartTime "
}; 
 
const char *sql_media_order_most_recently_written[] =
{
   /* MySQL */
   "ORDER BY LastWritten IS NULL,LastWritten DESC, MediaId",
   /* PostgreSQL */
   "ORDER BY LastWritten IS NULL,LastWritten DESC, MediaId",
   /* SQLite */
   "ORDER BY LastWritten IS NULL,LastWritten DESC, MediaId"
}; 
 
const char *sql_get_max_connections[] =
{
   /* MySQL */
   "SHOW VARIABLES LIKE 'max_connections'",
   /* PostgreSQL */
   "SHOW max_connections",
   /* SQLite */
   "SELECT  0"
};

/*
 *  The Group By can return strange numbers when having multiple
 *  version of a file in the same dataset.
 */
const char *default_sql_bvfs_select =
"CREATE TABLE %s AS"
"SELECT File.JobId, File.FileIndex, File.FileId "
"FROM Job, File, ( "
    "SELECT MAX(JobTDate) AS JobTDate, PathId, Filename "
       "FROM btemp%s GROUP BY PathId, Filename "
    ") AS T1 "
"WHERE T1.JobTDate = Job.JobTDate "
  "AND Job.JobId = File.JobId "
  "AND T1.PathId = File.PathId "
  "AND T1.Filename = File.Filename "
  "AND File.FileIndex > 0 "
  "AND Job.JobId IN (SELECT DISTINCT JobId FROM btemp%s) ";

const char *default_sql_bvfs_select_mysql =
"CREATE TABLE %s /*PKEY (DummyPkey INTEGER AUTO_INCREMENT PRIMARY KEY)*/ AS "
"SELECT File.JobId, File.FileIndex, File.FileId "
"FROM Job, File, ( "
    "SELECT MAX(JobTDate) AS JobTDate, PathId, Filename "
       "FROM btemp%s GROUP BY PathId, Filename "
    ") AS T1 "
"WHERE T1.JobTDate = Job.JobTDate "
  "AND Job.JobId = File.JobId "
  "AND T1.PathId = File.PathId "
  "AND T1.Filename = File.Filename "
  "AND File.FileIndex > 0 "
  "AND Job.JobId IN (SELECT DISTINCT JobId FROM btemp%s) ";

const char *sql_bvfs_select[] =
{
   /* MySQL */
   default_sql_bvfs_select_mysql,
   /* PostgreSQL */
   "CREATE TABLE %s AS ( "
        "SELECT JobId, FileIndex, FileId "
          "FROM ( "
             "SELECT DISTINCT ON (PathId, Filename) "
                    "JobId, FileIndex, FileId "
               "FROM btemp%s "
              "ORDER BY PathId, Filename, JobTDate DESC "
          ") AS T "
          "WHERE FileIndex > 0)",
   /* SQLite */
   default_sql_bvfs_select
};

static const char *sql_bvfs_list_files_default = 
"SELECT 'F', T.PathId, T.Filename, "
        "File.JobId, File.LStat, File.FileId "
"FROM Job, File, ( "
    "SELECT MAX(JobTDate) AS JobTDate, PathId, Filename "
      "FROM ( "
        "SELECT JobTDate, PathId, Filename "
          "FROM File JOIN Job USING (JobId) "
         "WHERE File.JobId IN (%s) AND PathId = %s "
          "UNION ALL "
        "SELECT JobTDate, PathId, Filename "
          "FROM BaseFiles "
               "JOIN File USING (FileId) "
               "JOIN Job  ON    (BaseJobId = Job.JobId) "
         "WHERE BaseFiles.JobId IN (%s)   AND PathId = %s "
       ") AS tmp GROUP BY PathId, Filename "
     "LIMIT %lld OFFSET %lld"
    ") AS T "
"WHERE T.JobTDate = Job.JobTDate "
  "AND Job.JobId = File.JobId "
  "AND T.PathId = File.PathId "
  "AND T.Filename = File.Filename "
  "AND File.Filename != '' "
  "AND File.FileIndex > 0 "
  " %s "                     /* AND Name LIKE '' */
  "AND (Job.JobId IN ( "
        "SELECT DISTINCT BaseJobId FROM BaseFiles WHERE JobId IN (%s)) "
       "OR Job.JobId IN (%s)) ";

const char *sql_bvfs_list_files[] = {
   /* MySQL */
   sql_bvfs_list_files_default,

   /* PostgreSQL */
 "SELECT Type, PathId, Filename, JobId, LStat, A.FileId "
  "FROM ("
   "SELECT DISTINCT ON (Filename) 'F' as Type, PathId, FileId, "
    "T.Filename, JobId, LStat, FileIndex "
     "FROM "
         "(SELECT FileId, JobId, PathId, Filename, FileIndex, LStat, MD5 "
            "FROM File WHERE JobId IN (%s) AND PathId = %s "
           "UNION ALL "
          "SELECT File.FileId, File.JobId, PathId, Filename, "
                 "File.FileIndex, LStat, MD5 "
            "FROM BaseFiles JOIN File USING (FileId) "
           "WHERE BaseFiles.JobId IN (%s) AND File.PathId = %s "
          ") AS T JOIN Job USING (JobId) "
          " WHERE T.Filename != '' "
          " %s "               /* AND Filename LIKE '' */
     "ORDER BY Filename, StartTime DESC "
   ") AS A WHERE A.FileIndex > 0 "
   "LIMIT %lld OFFSET %lld ",

   /* SQLite */
   sql_bvfs_list_files_default,

   /* SQLite */
   sql_bvfs_list_files_default
};

/* Basically the same thing than select_recent_version_with_basejob_and_delta_default,
 * but we specify a single file with Filename/PathId
 *
 * Input:
 * 1 JobId to look at
 * 2 Filename
 * 3 PathId
 * 4 JobId to look at
 * 5 Filename
 * 6 PathId
 * 7 Jobid
 * 8 JobId
 */
const char *bvfs_select_delta_version_with_basejob_and_delta_default =
"SELECT FileId, Job.JobId AS JobId, FileIndex, File.PathId AS PathId, "
       "File.Filename AS Filename, LStat, MD5, File.DeltaSeq AS DeltaSeq, "
       "Job.JobTDate AS JobTDate "
"FROM Job, File, ( "
    "SELECT MAX(JobTDate) AS JobTDate, PathId, Filename, DeltaSeq "
      "FROM ( "
       "SELECT JobTDate, PathId, Filename, DeltaSeq " /*Get all normal files*/
         "FROM File JOIN Job USING (JobId) "          /* from selected backup */
        "WHERE File.JobId IN (%s) AND Filename = '%s' AND PathId = %s "
         "UNION ALL "
       "SELECT JobTDate, PathId, Filename, DeltaSeq " /*Get all files from */
         "FROM BaseFiles "                            /* BaseJob */
              "JOIN File USING (FileId) "
              "JOIN Job  ON    (BaseJobId = Job.JobId) "
        "WHERE BaseFiles.JobId IN (%s) "        /* Use Max(JobTDate) to find */
             " AND Filename = '%s' AND PathId = %s "
       ") AS tmp "
       "GROUP BY PathId, Filename, DeltaSeq "    /* the latest file version */
    ") AS T1 "
"WHERE (Job.JobId IN ( "  /* Security, we force JobId to be valid */
        "SELECT DISTINCT BaseJobId FROM BaseFiles WHERE JobId IN (%s)) "
        "OR Job.JobId IN (%s)) "
  "AND T1.JobTDate = Job.JobTDate " /* Join on JobTDate to get the orginal */
  "AND Job.JobId = File.JobId "     /* Job/File record */
  "AND T1.PathId = File.PathId "
  "AND T1.Filename = File.Filename";


const char *bvfs_select_delta_version_with_basejob_and_delta[] =
{
   /* MySQL */
   bvfs_select_delta_version_with_basejob_and_delta_default,

   /* PostgreSQL */    /* The DISTINCT ON () permits to avoid extra join */
   "SELECT DISTINCT ON (Filename, PathId, DeltaSeq) JobTDate, JobId, FileId, "
         "FileIndex, PathId, Filename, LStat, MD5, DeltaSeq "
   "FROM "
    "(SELECT FileId, JobId, PathId, Filename, FileIndex, LStat, MD5,DeltaSeq "
         "FROM File WHERE JobId IN (%s) AND Filename = '%s' AND PathId = %s "
        "UNION ALL "
       "SELECT File.FileId, File.JobId, PathId, Filename, "
              "File.FileIndex, LStat, MD5, DeltaSeq "
         "FROM BaseFiles JOIN File USING (FileId) "
        "WHERE BaseFiles.JobId IN (%s) AND Filename = '%s' AND PathId = %s "
       ") AS T JOIN Job USING (JobId) "
   "ORDER BY Filename, PathId, DeltaSeq, JobTDate DESC ",

   /* SQLite */
   bvfs_select_delta_version_with_basejob_and_delta_default
};


const char *batch_lock_path_query[] =
{
   /* MySQL */
   "LOCK TABLES Path write,batch write,Path as p write",
   /* PostgreSQL */
   "BEGIN; LOCK TABLE Path IN SHARE ROW EXCLUSIVE MODE ",
   /* SQLite */
   "BEGIN "
}; 
 
const char *batch_unlock_tables_query[] =
{
   /* MySQL */
   "UNLOCK TABLES", 
   /* PostgreSQL */
   "COMMIT", 
   /* SQLite */
   "COMMIT" 
}; 
 
const char *batch_fill_path_query[] =
{
   /* MySQL */
   "INSERT INTO Path (Path)"
      "SELECT a.Path FROM "
         "(SELECT DISTINCT Path FROM batch) AS a WHERE NOT EXISTS "
         "(SELECT Path FROM Path AS p WHERE p.Path = a.Path)",
 
   /* PostgreSQL */
   "INSERT INTO Path (Path)"
      "SELECT a.Path FROM "
         "(SELECT DISTINCT Path FROM batch) AS a "
       "WHERE NOT EXISTS (SELECT Path FROM Path WHERE Path = a.Path) ",
 
   /* SQLite */
   "INSERT INTO Path (Path)"
      "SELECT DISTINCT Path FROM batch "
      "EXCEPT SELECT Path FROM Path"
}; 
 
const char *match_query[] =
{
   /* MySQL */
   "REGEXP",
   /* PostgreSQL */
   "~", 
   /* SQLite */
   "LIKE"                       /* MATCH doesn't seems to work anymore... */
}; 
 
static const char *insert_counter_values_default =
   "INSERT INTO Counters (Counter, MinValue, "
   "MaxValue, CurrentValue, WrapCounter) "
   "VALUES ('%s','%d','%d','%d','%s')";

const char *insert_counter_values[] = {
   /* MySQL */
   "INSERT INTO Counters (Counter, Counters.MinValue, "
   "Counters.MaxValue, CurrentValue, WrapCounter) "
   "VALUES ('%s','%d','%d','%d','%s')",

   /* PostgreSQL */
   insert_counter_values_default,

   /* SQLite */
   insert_counter_values_default
};

static const char *select_counter_values_default = 
   "SELECT MinValue, MaxValue, CurrentValue, WrapCounter"
   " FROM Counters WHERE Counter='%s'";
 
const char *select_counter_values[] =
{
   /* MySQL */
   "SELECT Counters.MinValue, Counters.MaxValue, CurrentValue, WrapCounter"
   " FROM Counters WHERE Counter='%s'",
 
   /* PostgreSQL */
   select_counter_values_default, 
 
   /* SQLite */
   select_counter_values_default 
}; 
 
static const char *update_counter_values_default = 
   "UPDATE Counters SET MinValue=%d, MaxValue=%d, CurrentValue=%d,"
    "WrapCounter='%s' WHERE Counter='%s'";
 
const char *update_counter_values[] =
{
   /* MySQL */
   "UPDATE Counters SET Counters.MinValue=%d, Counters.MaxValue=%d,"
     "CurrentValue=%d, WrapCounter='%s' WHERE Counter='%s'",
   /* PostgreSQL */
   update_counter_values_default, 
   /* SQLite */
   update_counter_values_default 
}; 

const char *prune_cache[] = {
   /* MySQL */
   " (Media.LastWritten + Media.CacheRetention) < NOW() ",
   /* PostgreSQL */
   " (Media.LastWritten + (interval '1 second' * Media.CacheRetention)) < NOW() ",
   /* SQLite */
   " ( (strftime('%s', Media.LastWritten) + Media.CacheRetention - strftime('%s', datetime('now', 'localtime'))) < 0) "
};

const char *expired_volumes[] = {
   /* MySQL */
"SELECT Media.VolumeName  AS volumename,"
       "Media.LastWritten AS lastwritten"
" FROM  Media"
" WHERE VolStatus IN ('Full', 'Used')"
     " AND Media.VolRetention > 0 "
     " AND ( Media.LastWritten +  Media.VolRetention ) < NOW()"
     " %s ",
   /* PostgreSQL */
   "SELECT Media.VolumeName, Media.LastWritten "
   " FROM  Media "
   " WHERE VolStatus IN ('Full', 'Used') "
     " AND Media.VolRetention > 0 "
     " AND ( Media.LastWritten + (interval '1 second' * Media.VolRetention ) < NOW()) "
     " %s ",
   /* SQLite */
"SELECT Media.VolumeName  AS volumename,"
       "Media.LastWritten AS lastwritten"
" FROM  Media"
" WHERE VolStatus IN ('Full', 'Used')"
     " AND Media.VolRetention > 0 "
     " AND ((strftime('%%s', LastWritten) + Media.VolRetention - strftime('%%s', datetime('now', 'localtime'))) < 0) "
     " %s "
};

const char *expires_in[] = {
   /* MySQL */
   "(GREATEST(0, CAST(UNIX_TIMESTAMP(LastWritten) + Media.VolRetention AS SIGNED) - UNIX_TIMESTAMP(NOW())))",
   /* PostgreSQL */
   "GREATEST(0, (extract('epoch' from LastWritten + Media.VolRetention * interval '1second' - NOW())::bigint))",
   /* SQLite */
   "MAX(0, (strftime('%s', LastWritten) + Media.VolRetention - strftime('%s', datetime('now', 'localtime'))))"
};

const char *strip_restore[] = {
   /* MySQL */
   "DELETE FROM %s WHERE FileId IN (SELECT * FROM (SELECT FileId FROM %s as B JOIN File USING (FileId) WHERE PathId IN (%s)) AS C)",
   /* PostgreSQL */
   "DELETE FROM %s WHERE FileId IN (SELECT FileId FROM %s JOIN File USING (FileId) WHERE PathId IN (%s))",
   /* SQLite */
   "DELETE FROM %s WHERE FileId IN (SELECT FileId FROM %s JOIN File USING (FileId) WHERE PathId IN (%s))"
};

const char *poolbytes[] = {
   /* MySQL */
   "COALESCE((SELECT SUM(VolBytes+VolABytes) FROM Media WHERE Media.PoolId=Pool.PoolId), 0)",
   /* PostgreSQL */
   "COALESCE((SELECT SUM(VolBytes+VolABytes) FROM Media WHERE Media.PoolId=Pool.PoolId), 0)::bigint",
   /* SQLite */
   "COALESCE((SELECT SUM(VolBytes+VolABytes) FROM Media WHERE Media.PoolId=Pool.PoolId), 0)"
};

const char *count_all_jobs = "SELECT COUNT(1) FROM Job";
const char *count_success_jobs = "SELECT COUNT(1) FROM Job WHERE JobStatus IN ('T', 'I') AND JobErrors=0";
const char *count_success_jobids = "SELECT COUNT(1) FROM Job WHERE JobStatus IN ('T', 'I') AND JobErrors=0 and JobId in (%s)";
const char *count_error_jobs = "SELECT COUNT(1) FROM Job WHERE JobStatus IN ('E','f','A')";
const char *count_error_jobids = "SELECT COUNT(1) FROM Job WHERE JobStatus IN ('E','f','A') and JobId in (%s)";
const char *count_warning_jobs = "SELECT COUNT(1) FROM Job WHERE JobStatus IN ('T','I') AND NOT JobErrors=0";
const char *count_warning_jobids = "SELECT COUNT(1) FROM Job WHERE JobStatus IN ('T','I') AND NOT JobErrors=0 and JobId in (%s)";
const char *sum_jobs_bytes = "SELECT SUM(JobBytes) FROM Job";
const char *sum_jobids_bytes = "SELECT SUM(JobBytes) FROM Job WHERE JobId in (%s)";
const char *sum_jobs_files = "SELECT SUM(JobFiles) FROM Job";
const char *sum_jobids_files = "SELECT SUM(JobFiles) FROM Job WHERE JobId in (%s)";
const char *count_all_volumes = "SELECT COUNT(1) FROM Media";
const char *count_avl_volumes = "SELECT COUNT(1) FROM Media WHERE VolStatus='Append'";
const char *count_error_volumes = "SELECT COUNT(1) FROM Media WHERE VolStatus='Error'";
const char *count_full_volumes = "SELECT COUNT(1) FROM Media WHERE VolStatus='Full'";
const char *count_used_volumes = "SELECT COUNT(1) FROM Media WHERE VolStatus='Used'";
const char *sum_volumes_bytes = "SELECT SUM(VolBytes) FROM Media";
const char *get_volume_size = "SELECT VolBytes FROM Media WHERE VolumeName='%s'";

static const char *escape_char_value_default = "\\";

const char *escape_char_value[] = {
   /* MySQL */
   "\\\\",
   /* PostgreSQL */
   escape_char_value_default,
   /* SQLite */
   escape_char_value_default
};

static const char *regexp_value_default = "REGEXP";

const char *regexp_value[] = {
   /* MySQL */
   regexp_value_default,
   /* PostgreSQL */
   "~",
   /* SQLite */
   regexp_value_default
};
