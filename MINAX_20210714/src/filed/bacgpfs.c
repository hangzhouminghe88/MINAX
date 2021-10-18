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
 * This is a Bacula support for GPFS.
 * Author: Radoslaw Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#include "bacula.h"

#if defined(HAVE_GPFS_ACL)
   #include <gpfs.h>
#endif

#include "filed.h"

#ifdef HAVE_SYS_TYPES_H
   #include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
   #include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
   #include <unistd.h>
#endif

#ifdef HAVE_DLFCN_H
   #include <dlfcn.h>
#endif


#if defined(HAVE_LINUX_OS)
   #define GPFSLIB_LIB        "libgpfs.so"
   #define GPFSLIB_PATH       "/usr/lpp/mmfs/lib/libgpfs.so"
#elif defined(HAVE_AIX_OS)
   #define GPFSLIB_LIB        "libgpfs.a"
   #define GPFSLIB_PATH       "/usr/lpp/mmfs/lib/libgpfs.a"
#endif


/**
 * @brief
 *
 */
void GPFSLIB::initialize()
{
#if defined(HAVE_GPFS_ACL) && defined(HAVE_DLFCN_H)
#ifdef HAVE_LINUX_OS

   /**
    * @brief The static location of the library and search list
    */
   static const char * gpfslib_paths[] =
   {
#if defined(GPFSLIB_CUSTOM_PATH)
      GPFSLIB_CUSTOM_PATH,
#endif
      GPFSLIB_PATH,
      GPFSLIB_LIB,
      NULL,
   };

   const char * gpfs_path;
   bool still_search = true;
   int path_indx = 0;
   const char * _path = gpfslib_paths[path_indx];

   // iterate through all library locations
   while (still_search){
      gpfs_path = _path;
      Dmsg1(200, "Try to load GPFS library at: %s\n", gpfs_path);
      handle = dlopen(gpfs_path, RTLD_NOW | RTLD_GLOBAL);
      _path = gpfslib_paths[++path_indx];
      still_search = handle == NULL && _path != NULL;
   }

   // check if dlopen was successful in some point
   if (handle != NULL){
      Dmsg1(200, "Loaded GPFS library: %s\n", gpfs_path);
      // resolve required symbols

      _gpfs_getacl = (_gpfs_getacl_func_t) dlsym(handle, GPFSLIB_SYM_GETACL);
      // check symbol resolution
      if (_gpfs_getacl != NULL){
         _gpfs_putacl = (_gpfs_putacl_func_t) dlsym(handle, GPFSLIB_SYM_PUTACL);
         // check symbol resolution
         if (_gpfs_putacl != NULL){
            Dmsg4(200, "Successfully mapped symbols: %s=%p %s=%p\n",
                        GPFSLIB_SYM_GETACL, _gpfs_getacl,
                        GPFSLIB_SYM_PUTACL, _gpfs_putacl);
            return;
         }
      }

      // error rollback all operations
      release();

   } else {
      // show error if any
#ifdef HAVE_DLERROR
         char *err = dlerror();
         if (err != NULL){
            Dmsg2(1, "Error loading GPFS Library: %s, Err=%s\n", GPFSLIB_PATH, NPRT(err));
         }
#else
         Dmsg1(1, "Error loading GPFS Library: %s!\n", GPFSLIB_PATH);
#endif

   }

#endif
#endif
};

/**
 * @brief
 *
 */
void GPFSLIB::release()
{
   if (handle != NULL){
      int rc = 0;

      _gpfs_getacl = NULL;
      _gpfs_putacl = NULL;

#ifdef HAVE_DLFCN_H
      rc = dlclose(handle);
#endif

      if (rc != 0){

#ifdef HAVE_DLERROR
         char *err = dlerror();
         if (err != NULL){
            Dmsg1(1, "Error releasing GPFS Library, Err=%s\n", NPRT(err));
         }
#else
         Dmsg0(1, "Error releasing GPFS Library!\n");
#endif

      }
      handle = NULL;
   }
};

/**
 * @brief
 *
 * @param jcr
 * @param ff_pkt
 * @param acl_type
 * @param content
 * @param content_len
 * @return bRC_GPFSLIB
 */
bRC_GPFSLIB GPFSLIB::_gpfs_backup_acl_data(JCR *jcr, FF_PKT *ff_pkt, unsigned char acl_type, POOLMEM *content, uint32_t &content_len)
{
#if defined(HAVE_GPFS_ACL)
   int rc;

   content_len = 0;
   if (_gpfs_getacl != NULL){
      /*
      * first we have to check what space is required,
      * this is achieved by forcing smal sized buffer
      */
      gpfs_opaque_acl_t acltmp;
      acltmp.acl_buffer_len = sizeof(gpfs_opaque_acl_t);
      acltmp.acl_type = acl_type;
      rc = _gpfs_getacl(ff_pkt->fname, 0, &acltmp);

      /*
       * We expect that the first gpfs_getacl will return with error when it will have at least a single acl entry,
       * and assume if there is no error then no acl entry available.
       */
      if (rc < 0){
         berrno be;
         gpfs_opaque_acl_t * aclp;
         int acl_len;

         switch (be.code()){
            case ENOSPC:
               // size of required buffer is at acltmp.acl_buffer_len
               acl_len = acltmp.acl_buffer_len + sizeof(gpfs_opaque_acl_t);

               // prepare required buffer
               content = check_pool_memory_size(content, acl_len);

               // fill required data
               aclp = (gpfs_opaque_acl_t*)content;
               aclp->acl_buffer_len = acl_len;
               aclp->acl_version = 0;
               aclp->acl_type = acl_type;

               rc = _gpfs_getacl(ff_pkt->fname, 0, content);
               if (rc < 0){
                  // error reading acl
                  berrno be;
                  switch (be.code()){
                     case ENOENT:
                     case ENODATA:
                        return bRC_GPFSLIB_ok;

                     default:
                        Mmsg2(jcr->errmsg, _("gpfs_getacl error on file \"%s\": ERR=%s\n"), ff_pkt->fname, be.bstrerror());
                        Dmsg2(100, "gpfs_getacl error file=%s ERR=%s\n", ff_pkt->fname, be.bstrerror());
                        return bRC_GPFSLIB_error;
                  }
               }
               content_len = acl_len;
               break;

            case ENOENT:
            case EINVAL:
            case ENODATA:
               return bRC_GPFSLIB_ok;

            default:
               // this is a real error, report
               Mmsg2(jcr->errmsg, _("gpfs_getacl error on file \"%s\": ERR=%s\n"), ff_pkt->fname, be.bstrerror());
               Dmsg2(100, "gpfs_getacl error file=%s ERR=%s\n", ff_pkt->fname, be.bstrerror());
               return bRC_GPFSLIB_error;
         }
      }
   }
#endif

   return bRC_GPFSLIB_ok;
};

/**
 * @brief
 *
 * @param jcr
 * @param stream
 * @param content
 * @param content_len
 * @return bRC_GPFSLIB
 */
bRC_GPFSLIB GPFSLIB::_gpfs_restore_acl_data(JCR *jcr, int stream, POOLMEM *content, uint32_t content_len)
{
#if defined(HAVE_GPFS_ACL)
   int rc;

   if (_gpfs_putacl != NULL){
      /* check input data */
      if (jcr == NULL || content == NULL){
         return bRC_GPFSLIB_fatal;
      }

      if (content_len > 0 && jcr->last_type != FT_LNK){
         rc = _gpfs_putacl(jcr->last_fname, 0, content);
         if (rc < 0){
            // error saving acl
            berrno be;
            switch (be.code()){
               case ENOSYS:

                  return bRC_GPFSLIB_error;
               default:
                  // this is a real error, report
                  Mmsg2(jcr->errmsg, _("gpfs_getacl error on file \"%s\": ERR=%s\n"), jcr->last_fname, be.bstrerror());
                  Dmsg2(100, "gpfs_getacl error file=%s ERR=%s\n", jcr->last_fname, be.bstrerror());
                  return bRC_GPFSLIB_error;
            }
         }
      }
   }
#endif

   return bRC_GPFSLIB_ok;
};
