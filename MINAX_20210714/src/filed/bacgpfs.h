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

#ifndef _BAC_GPFS_H_
#define _BAC_GPFS_H_

/* Coming from gpfs.h */
#ifndef GPFS_ACL_TYPE_ACCESS
#define GPFS_ACL_TYPE_ACCESS     1
#endif
#ifndef GPFS_ACL_TYPE_DEFAULT
#define GPFS_ACL_TYPE_DEFAULT    2
#endif
#ifndef GPFS_ACL_TYPE_NFS4
#define GPFS_ACL_TYPE_NFS4       3
#endif

#ifndef GPFS_SUPER_MAGIC
#define GPFS_SUPER_MAGIC     0x47504653
#endif


/*
 * Define a function pointer types for better pointer handling
 */
typedef int (*_gpfs_getacl_func_t)(char *pathname, int flags, void *aclP);
typedef int (*_gpfs_putacl_func_t)(char *pathname, int flags, void *aclP);

#define  GPFSLIB_SYM_GETACL      "gpfs_getacl"
#define  GPFSLIB_SYM_PUTACL      "gpfs_putacl"

enum bRC_GPFSLIB
{
   bRC_GPFSLIB_ok,
   bRC_GPFSLIB_error,
   bRC_GPFSLIB_fatal,
};

/**
 * @brief
 *
 */
class GPFSLIB : public SMARTALLOC
{
#if defined(HAVE_LINUX_OS) || defined(HAVE_AIX_OS)
   _gpfs_getacl_func_t _gpfs_getacl;               // this is function pointer
   _gpfs_putacl_func_t _gpfs_putacl;
#else
   void * _gpfs_getacl;
   void * _gpfs_putacl;
#endif

   void *handle;                                                  // this is a dlopen handle

   /**
    * @brief Construct a new GPFSLIB object
    *
    */
   GPFSLIB() : _gpfs_getacl(NULL), _gpfs_putacl(NULL), handle(NULL) {};
#if __cplusplus > 201103L
   GPFSLIB(const GPFSLIB&) = delete;
#endif

   void initialize();
   void release();
   bRC_GPFSLIB _gpfs_backup_acl_data(JCR *jcr, FF_PKT *ff_pkt, unsigned char acl_type, POOLMEM *content, uint32_t &content_len);
   bRC_GPFSLIB _gpfs_restore_acl_data(JCR *jcr, int stream, POOLMEM *content, uint32_t content_len);


public:

   /* Check if the library is loaded */
   static bool enabled() {
      return Get()._gpfs_getacl != NULL;
   };

   /**
    * @brief Destroy the GPFSLIB object
    *
    */
   ~GPFSLIB()
   {
      release();
   };

   /**
    * @brief This is a main singleton method of the GPFSLIB class.
    *    The Singleton is a class compound with an object instance instantinated in this
    *    compilation section, see: https://en.wikipedia.org/wiki/Singleton_pattern
    *
    * @return GPFSLIB& a reference to the singleton object of GPFSLIB
    */
   static GPFSLIB& Get()
   {
      static GPFSLIB _singleton_instance;
      return _singleton_instance;
   };

   /**
    * @brief
    *
    */
   static void Init() { Get().initialize(); };

   /**
    * @brief
    *
    */
   static void Release() { Get().release(); };

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
   static bRC_GPFSLIB gpfs_backup_acl_data(JCR *jcr, FF_PKT *ff_pkt, unsigned char acl_type, POOLMEM *content, uint32_t &content_len)
   {
      return Get()._gpfs_backup_acl_data(jcr, ff_pkt, acl_type, content, content_len);
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
   static bRC_GPFSLIB gpfs_restore_acl_data(JCR *jcr, int stream, POOLMEM *content, uint32_t content_len)
   {
      return Get()._gpfs_restore_acl_data(jcr, stream, content, content_len);
   };

};

#endif   /* _BAC_GPFS_H_ */
