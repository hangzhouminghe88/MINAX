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
 * Routines for writing to the Cloud using S3 protocol.
 *  NOTE!!! This cloud driver is not compatible with
 *   any disk-changer script for changing Volumes.
 *   It does however work with Bacula Virtual autochangers.
 *
 * Written by Kern Sibbald, May MMXVI
 *
 */

#include "s3_driver.h"
#ifdef BEE
#include "bee_s3_cloud_glacier.h"
#endif
#include <dlfcn.h>
#include <fcntl.h>

#ifdef HAVE_LIBS3

static const int64_t dbglvl = DT_CLOUD|50;
static const char *S3Errors[] = {
   "OK",
   "InternalError",
   "OutOfMemory",
   "Interrupted",
   "InvalidBucketNameTooLong",
   "InvalidBucketNameFirstCharacter",
   "InvalidBucketNameCharacter",
   "InvalidBucketNameCharacterSequence",
   "InvalidBucketNameTooShort",
   "InvalidBucketNameDotQuadNotation",
   "QueryParamsTooLong",
   "FailedToInitializeRequest",
   "MetaDataHeadersTooLong",
   "BadMetaData",
   "BadContentType",
   "ContentTypeTooLong",
   "BadMD5",
   "MD5TooLong",
   "BadCacheControl",
   "CacheControlTooLong",
   "BadContentDispositionFilename",
   "ContentDispositionFilenameTooLong",
   "BadContentEncoding",
   "ContentEncodingTooLong",
   "BadIfMatchETag",
   "IfMatchETagTooLong",
   "BadIfNotMatchETag",
   "IfNotMatchETagTooLong",
   "HeadersTooLong",
   "KeyTooLong",
   "UriTooLong",
   "XmlParseFailure",
   "EmailAddressTooLong",
   "UserIdTooLong",
   "UserDisplayNameTooLong",
   "GroupUriTooLong",
   "PermissionTooLong",
   "TargetBucketTooLong",
   "TargetPrefixTooLong",
   "TooManyGrants",
   "BadGrantee",
   "BadPermission",
   "XmlDocumentTooLarge",
   "NameLookupError",
   "FailedToConnect",
   "ServerFailedVerification",
   "ConnectionFailed",
   "AbortedByCallback",
   "AccessDenied",
   "AccountProblem",
   "AmbiguousGrantByEmailAddress",
   "BadDigest",
   "BucketAlreadyExists",
   "BucketAlreadyOwnedByYou",
   "BucketNotEmpty",
   "CredentialsNotSupported",
   "CrossLocationLoggingProhibited",
   "EntityTooSmall",
   "EntityTooLarge",
   "ExpiredToken",
   "IllegalVersioningConfigurationException",
   "IncompleteBody",
   "IncorrectNumberOfFilesInPostRequest",
   "InlineDataTooLarge",
   "ErrorInternalError",
   "InvalidAccessKeyId",
   "InvalidAddressingHeader",
   "InvalidArgument",
   "InvalidBucketName",
   "InvalidBucketState",
   "InvalidDigest",
   "InvalidLocationConstraint",
   "InvalidObjectState",
   "InvalidPart",
   "InvalidPartOrder",
   "InvalidPayer",
   "InvalidPolicyDocument",
   "InvalidRange",
   "InvalidRequest",
   "InvalidSecurity",
   "InvalidSOAPRequest",
   "InvalidStorageClass",
   "InvalidTargetBucketForLogging",
   "InvalidToken",
   "InvalidURI",
   "KeyTooLong",
   "MalformedACLError",
   "MalformedPOSTRequest",
   "MalformedXML",
   "MaxMessageLengthExceeded",
   "MaxPostPreDataLengthExceededError",
   "MetadataTooLarge",
   "MethodNotAllowed",
   "MissingAttachment",
   "MissingContentLength",
   "MissingRequestBodyError",
   "MissingSecurityElement",
   "MissingSecurityHeader",
   "NoLoggingStatusForKey",
   "NoSuchBucket",
   "NoSuchKey",
   "NoSuchLifecycleConfiguration",
   "NoSuchUpload",
   "NoSuchVersion",
   "NotImplemented",
   "NotSignedUp",
   "NotSuchBucketPolicy",
   "OperationAborted",
   "PermanentRedirect",
   "PreconditionFailed",
   "Redirect",
   "RestoreAlreadyInProgress",
   "RequestIsNotMultiPartContent",
   "RequestTimeout",
   "RequestTimeTooSkewed",
   "RequestTorrentOfBucketError",
   "SignatureDoesNotMatch",
   "ServiceUnavailable",
   "SlowDown",
   "TemporaryRedirect",
   "TokenRefreshRequired",
   "TooManyBuckets",
   "UnexpectedContent",
   "UnresolvableGrantByEmailAddress",
   "UserKeyMustBeSpecified",
   "Unknown",
   "HttpErrorMovedTemporarily",
   "HttpErrorBadRequest",
   "HttpErrorForbidden",
   "HttpErrorNotFound",
   "HttpErrorConflict",
   "HttpErrorUnknown",
   "Undefined"
};

#define S3ErrorsSize (sizeof(S3Errors)/sizeof(char *))

static void load_glacier_driver(const char* plugin_directory);

#ifdef __cplusplus
extern "C" {
#endif

cloud_driver *BaculaCloudDriver()
{
   return New(s3_driver);
}

typedef cloud_glacier *(*newGlacierDriver_t)(void);

/* Needed for bcloud utility (backdoor glacier driver pre-loading) */
void BaculaInitGlacier(const char* plugin_directory)
{
   load_glacier_driver(plugin_directory);
}

#ifdef __cplusplus
}
#endif

struct glacier_driver_item {
   const char *name;
   void *handle;
   newGlacierDriver_t newDriver;
   cloud_glacier *ptr;
   bool builtin;
   bool loaded;
};

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

glacier_driver_item glacier_item = {0};

static void load_glacier_driver(const char* plugin_directory)
{
   if (!glacier_item.newDriver) {
      POOL_MEM fname(PM_FNAME);
      Mmsg(fname, "%s/bacula-sd-cloud-glacier-s3-driver-%s%s",  
         plugin_directory, VERSION, ".so");

      glacier_item.handle = dlopen(fname.c_str(), RTLD_NOW);
      if (glacier_item.handle) {
         glacier_item.newDriver = (newGlacierDriver_t)dlsym(glacier_item.handle, "BaculaCloudGlacier");
         if (!glacier_item.newDriver) {
            dlclose(glacier_item.handle);
            glacier_item.ptr = NULL;
            return;
         }
         glacier_item.ptr = glacier_item.newDriver();
      }
   }
}

static void unload_drivers()
{
   if (glacier_item.handle) {
      dlclose(glacier_item.handle);
   }
}

/*
 * Our Bacula context for s3_xxx callbacks
 *   NOTE: only items needed for particular callback are set
 */
class bacula_ctx {
public:
   cancel_callback *cancel_cb;
   transfer *xfer;
   POOLMEM *&errMsg;
   ilist *parts;
   int isTruncated;
   char* nextMarker;
   int64_t obj_len;
   const char *caller;
   FILE *infile;
   FILE *outfile;
   alist *volumes;
   S3Status status;
   bwlimit *limit;        /* Used to control the bandwidth */
   cleanup_cb_type *cleanup_cb;
   cleanup_ctx_type *cleanup_ctx;
   bool isRestoring;
   bacula_ctx(POOLMEM *&err) : cancel_cb(NULL), xfer(NULL), errMsg(err), parts(NULL),
                              isTruncated(0), nextMarker(NULL), obj_len(0), caller(NULL),
                              infile(NULL), outfile(NULL), volumes(NULL), status(S3StatusOK),
                              limit(NULL), cleanup_cb(NULL), cleanup_ctx(NULL), isRestoring(false)
   {
      /* reset error message (necessary in case of retry) */
      errMsg[0] = 0;
   }
   bacula_ctx(transfer *t) : cancel_cb(NULL), xfer(t), errMsg(t->m_message), parts(NULL),
                              isTruncated(0), nextMarker(NULL), obj_len(0), caller(NULL),
                              infile(NULL), outfile(NULL), volumes(NULL), status(S3StatusOK),
                              limit(NULL), cleanup_cb(NULL), cleanup_ctx(NULL), isRestoring(false)
   {
      /* reset error message (necessary in case of retry) */
      errMsg[0] = 0;
   }
};


/* Imported functions */
const char *mode_to_str(int mode);

/* Forward referenced functions */

/* Const and Static definitions */

static S3Status responsePropertiesCallback(
   const S3ResponseProperties *properties,
   void *callbackData);

static void responseCompleteCallback(
   S3Status status,
   const S3ErrorDetails *oops,
   void *callbackData);


S3ResponseHandler responseHandler =
{
   &responsePropertiesCallback,
   &responseCompleteCallback
};

static S3Status responsePropertiesCallback(
   const S3ResponseProperties *properties,
   void *callbackData)
{
   bacula_ctx *ctx = (bacula_ctx *)callbackData;
   ASSERT(ctx);
   if (ctx->xfer && properties) {
      if (properties->contentLength > 0) {
         ctx->xfer->m_res_size = properties->contentLength;
      }
      if (properties->lastModified > 0) {
         ctx->xfer->m_res_mtime = properties->lastModified;
      }
      if (properties->restore) {
         const char *c = strchr( properties->restore, '"' );
         c++;
         /* t stand for true, all the rest is considered false */
         ctx->isRestoring = (*c=='t');
      }
   }
   return S3StatusOK;
}

static void responseCompleteCallback(
   S3Status status,
   const S3ErrorDetails *oops,
   void *callbackCtx)
{
   bacula_ctx *ctx = (bacula_ctx *)callbackCtx;
   const char *msg;

   Enter(dbglvl);
   if (ctx) {
      ctx->status = status;      /* return completion status */
   }
   if (status < 0 || status > S3ErrorsSize) {
      status = (S3Status)S3ErrorsSize;
   }
   msg = oops->message;
   if (!msg) {
      msg = S3Errors[status];
   }
   if ((status != S3StatusOK) && ctx->errMsg) 
   {
      Mmsg(ctx->errMsg, "%s ERR=%s", ctx->caller, msg);
      if (oops->furtherDetails) {
         pm_strcat(ctx->errMsg, " ");
         pm_strcat(ctx->errMsg, oops->furtherDetails);
      }
      if (oops->curlError) {
         pm_strcat(ctx->errMsg, " ");
         pm_strcat(ctx->errMsg, oops->curlError);
      }
      for (int i=0; i<oops->extraDetailsCount; ++i) {
         pm_strcat(ctx->errMsg, " ");
         pm_strcat(ctx->errMsg, oops->extraDetails[i].name);
         pm_strcat(ctx->errMsg, " : ");
         pm_strcat(ctx->errMsg, oops->extraDetails[i].value);
      }
   }
   return;
}

bool xfer_cancel_cb(void *arg)
{
   transfer *xfer = (transfer*)arg;
   if (xfer) {
      return xfer->is_canceled();
   }
   return false;
};

static int putObjectCallback(int buf_len, char *buf, void *callbackCtx)
{
   bacula_ctx *ctx = (bacula_ctx *)callbackCtx;

   ssize_t rbytes = 0;
   int read_len;

   if (ctx->xfer->is_canceled()) {
      Mmsg(ctx->errMsg, _("Job cancelled.\n"));
      return -1;
   }
   if (ctx->obj_len) {
      read_len = (ctx->obj_len > buf_len) ? buf_len : ctx->obj_len;
      rbytes = fread(buf, 1, read_len, ctx->infile);
      Dmsg6(dbglvl, "%s xfer=part.%lu thread=%lu rbytes=%d bufsize=%u remlen=%lu\n",
             ctx->caller, ctx->xfer->m_part, pthread_self(), rbytes, buf_len, ctx->obj_len);
      if (rbytes <= 0) {
         berrno be;
         Mmsg(ctx->errMsg, "%s Error reading input file: ERR=%s\n",
            ctx->caller, be.bstrerror());
         goto get_out;
      }
      ctx->obj_len -= rbytes;
      ctx->xfer->increment_processed_size(rbytes);
      if (ctx->limit) {
         ctx->limit->control_bwlimit(rbytes);
      }
   }

get_out:
   return rbytes;
}

S3PutObjectHandler putObjectHandler =
{
   responseHandler,
   &putObjectCallback
};

/*
 * Put a cache object into the cloud
 */
S3Status s3_driver::put_object(transfer *xfer, const char *cache_fname, const char *cloud_fname)
{
   Enter(dbglvl);
   bacula_ctx ctx(xfer);
   ctx.limit = upload_limit.use_bwlimit() ? &upload_limit : NULL;

   struct stat statbuf;
   if (lstat(cache_fname, &statbuf) == -1) {
      berrno be;
      Mmsg2(ctx.errMsg, "Failed to stat file %s. ERR=%s\n",
         cache_fname, be.bstrerror());
      goto get_out;
   }

   ctx.obj_len = statbuf.st_size;

   if (!(ctx.infile = bfopen(cache_fname, "r"))) {
      berrno be;
      Mmsg2(ctx.errMsg, "Failed to open input file %s. ERR=%s\n",
         cache_fname, be.bstrerror());
      goto get_out;
   }

   ctx.caller = "S3_put_object";
   S3_put_object(&s3ctx, cloud_fname, ctx.obj_len, NULL, NULL, 0,
               &putObjectHandler, &ctx);

get_out:
   if (ctx.infile) {
      fclose(ctx.infile);       /* input file */
   }

   /* no error so far -> retrieve uploaded part info */
   if (ctx.errMsg[0] == 0) {
      ilist parts;
      if (get_one_cloud_volume_part(cloud_fname, &parts, ctx.errMsg)) {
         /* only one part is returned */
         cloud_part *p = (cloud_part *)parts.get(parts.last_index());
         if (p) {
            xfer->m_res_size = p->size;
            xfer->m_res_mtime = p->mtime;
            bmemzero(xfer->m_hash64, 64);
         }
      }
   } else {
      Dmsg1(dbglvl, "put_object ERROR: %s\n", ctx.errMsg);
   }

   return ctx.status;
}

static S3Status getObjectDataCallback(int buf_len, const char *buf,
                   void *callbackCtx)
{
   bacula_ctx *ctx = (bacula_ctx *)callbackCtx;
   ssize_t wbytes;

   Enter(dbglvl);
   if (ctx->xfer->is_canceled()) {
       Mmsg(ctx->errMsg, _("Job cancelled.\n"));
       return S3StatusAbortedByCallback;
   }
   /* Write buffer to output file */
   wbytes = fwrite(buf, 1, buf_len, ctx->outfile);
   if (wbytes < 0) {
      berrno be;
      Mmsg(ctx->errMsg, "%s Error writing output file: ERR=%s\n",
         ctx->caller, be.bstrerror());
      return S3StatusAbortedByCallback;
   }
   ctx->xfer->increment_processed_size(wbytes);
   if (ctx->limit) {
      ctx->limit->control_bwlimit(wbytes);
   }
   return ((wbytes < buf_len) ?
            S3StatusAbortedByCallback : S3StatusOK);
}


int s3_driver::get_cloud_object(transfer *xfer, const char *cloud_fname, const char *cache_fname)
{
   int64_t ifModifiedSince = -1;
   int64_t ifNotModifiedSince = -1;
   const char *ifMatch = 0;
   const char *ifNotMatch = 0;
   uint64_t startByte = 0;
   uint64_t byteCount = 0;
   bacula_ctx ctx(xfer);
   ctx.limit = download_limit.use_bwlimit() ? &download_limit : NULL;
   bool retry = false;

   Enter(dbglvl);
   /* Initialize handlers */
   S3GetConditions getConditions = {
      ifModifiedSince,
      ifNotModifiedSince,
      ifMatch,
      ifNotMatch
   };
   S3GetObjectHandler getObjectHandler = {
     { &responsePropertiesCallback, &responseCompleteCallback },
       &getObjectDataCallback
   };


   /* see if cache file already exists */
   struct stat buf;
   if (lstat(cache_fname, &buf) == -1) {
       ctx.outfile = bfopen(cache_fname, "w");
   } else {
      /* Exists so truncate and write from beginning */
      ctx.outfile = bfopen(cache_fname, "r+");
   }

   if (!ctx.outfile) {
      berrno be;
      Mmsg2(ctx.errMsg, "Could not open cache file %s. ERR=%s\n",
              cache_fname, be.bstrerror());
      goto get_out;
   }


   ctx.caller = "S3_get_object";
   S3_get_object(&s3ctx, cloud_fname, &getConditions, startByte,
                 byteCount, NULL, 0, &getObjectHandler, &ctx);

   /* Archived objects (in GLACIER or DEEP_ARCHIVE) will return InvalidObjectStateError */
   retry = (ctx.status == S3StatusErrorInvalidObjectState);
   if (retry) {
      restore_cloud_object(xfer, cloud_fname);
   }

   if (fclose(ctx.outfile) < 0) {
      berrno be;
      Mmsg2(ctx.errMsg, "Error closing cache file %s: %s\n",
              cache_fname, be.bstrerror());
   }

get_out:
   if (retry) return CLOUD_DRIVER_COPY_PART_TO_CACHE_RETRY;
   return (ctx.errMsg[0] == 0) ? CLOUD_DRIVER_COPY_PART_TO_CACHE_OK : CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
}

bool s3_driver::move_cloud_part(const char *VolumeName, uint32_t apart, const char *to, cancel_callback *cancel_cb, POOLMEM *&err, int& exists)
{
   POOLMEM *cloud_fname = get_pool_memory(PM_FNAME);
   cloud_fname[0] = 0;
   make_cloud_filename(cloud_fname, VolumeName, apart);
   POOLMEM *dest_cloud_fname = get_pool_memory(PM_FNAME);
   dest_cloud_fname[0] = 0;
   add_vol_and_part(dest_cloud_fname, VolumeName, to, apart);
   int64_t lastModifiedReturn=0LL;
   bacula_ctx ctx(err);
   ctx.caller = "S3_copy_object";
   Dmsg3(dbglvl, "%s trying to move %s to %s\n", ctx.caller, cloud_fname, dest_cloud_fname);
   S3_copy_object(&s3ctx,                 //bucketContext
                  cloud_fname,            //key
                  NULL,                   //destinationBucket -> same
                  dest_cloud_fname,       // destinationKey
                  NULL,                   // putProperties
                  &lastModifiedReturn,
                  0,                      //eTagReturnSize
                  NULL,                   //eTagReturn
                  NULL,                   // requestContext=NULL -> process now
                  0,                      // timeoutMs
                  &responseHandler,       // handler
                  &ctx                    //callbackData
                  );
   free_pool_memory(dest_cloud_fname);
   free_pool_memory(cloud_fname);
   if (ctx.status == S3StatusOK) {
      exists = true;
      Mmsg(err, "%s", to);
      return true;
   } else if (ctx.status == S3StatusXmlParseFailure) {
      /* source doesn't exist. OK. */
      exists = false;
      err[0] = 0;
      return true;
   } else {
      return (err[0] == 0);
   }
}

/*
 * libs3 callback for clean_cloud_volume()
 */
static S3Status partsAndCopieslistBucketCallback(
   int isTruncated,
   const char *nextMarker,
   int numObj,
   const S3ListBucketContent *object,
   int commonPrefixesCount,
   const char **commonPrefixes,
   void *callbackCtx)
{
   bacula_ctx *ctx = (bacula_ctx *)callbackCtx;
   cleanup_ctx_type *cleanup_ctx = ctx->cleanup_ctx;
   cleanup_cb_type *cb = ctx->cleanup_cb;
   Enter(dbglvl);
   for (int i = 0; (cleanup_ctx && (i < numObj)); i++) {
      const S3ListBucketContent *obj = &(object[i]);
      if (obj && cb(obj->key, cleanup_ctx)) {
         ctx->parts->append(bstrdup(obj->key));
         Dmsg1(dbglvl, "partsAndCopieslistBucketCallback: %s retrieved\n", obj->key);
      }

      if (ctx->cancel_cb && ctx->cancel_cb->fct && ctx->cancel_cb->fct(ctx->cancel_cb->arg)) {
         Mmsg(ctx->errMsg, _("Job cancelled.\n"));
         return S3StatusAbortedByCallback;
      }
   }

   ctx->isTruncated = isTruncated;
   if (ctx->nextMarker) {
      bfree_and_null(ctx->nextMarker);
   }
   if (isTruncated && numObj>0) {
      /* Workaround a bug with nextMarker */
      const S3ListBucketContent *obj = &(object[numObj-1]);
      ctx->nextMarker = bstrdup(obj->key);
   }

   Leave(dbglvl);
   return S3StatusOK;
}

S3ListBucketHandler partsAndCopiesListBucketHandler =
{
   responseHandler,
   &partsAndCopieslistBucketCallback
};

/* remove part* from volume instead of part.* in truncate. Intended to remove copies created when part is overwritten */
/* If legit parts are still present in the volume, they will be deleted, except part.1 */
bool s3_driver::clean_cloud_volume(const char *VolumeName, cleanup_cb_type *cb, cleanup_ctx_type *context, cancel_callback *cancel_cb, POOLMEM *&err)
{
   Enter(dbglvl);

   if (strlen(VolumeName) == 0) {
      pm_strcpy(err, "Invalid argument");
      return false;
   }

   ilist parts;

   bacula_ctx ctx(err);
   ctx.cancel_cb = cancel_cb;
   ctx.parts = &parts;
   ctx.isTruncated = 1; /* pass into the while loop at least once */
   ctx.caller = "S3_list_bucket";
   ctx.cleanup_cb = cb;
   ctx.cleanup_ctx = context;

   while (ctx.isTruncated!=0) {
      ctx.isTruncated = 0;
      S3_list_bucket(&s3ctx, VolumeName, ctx.nextMarker, NULL, 0, NULL, 0, &partsAndCopiesListBucketHandler, &ctx);
      Dmsg4(dbglvl, "get_cloud_volume_parts_list isTruncated=%d, nextMarker=%s, nbparts=%d, err=%s\n", ctx.isTruncated, ctx.nextMarker, ctx.parts->size(), ctx.errMsg?ctx.errMsg:"None");
      if (ctx.status != S3StatusOK) {
         pm_strcpy(err, S3Errors[ctx.status]);
         bfree_and_null(ctx.nextMarker);
         return false;
      }
   }
   bfree_and_null(ctx.nextMarker);

   int last_index = (int)parts.last_index();
   for (int i=0; (i<=last_index); i++) {
      char *part = (char *)parts.get(i);
      if (!part) {
         continue;
      }
      if (cancel_cb && cancel_cb->fct && cancel_cb->fct(cancel_cb->arg)) {
         Mmsg(err, _("Job cancelled.\n"));
         return false;
      }
      /* don't forget to specify the volume name is the object path */
      Dmsg1(dbglvl, "Object to cleanup: %s\n", part);
      ctx.caller = "S3_delete_object";
      S3_delete_object(&s3ctx, part, NULL, 0, &responseHandler, &ctx);
      if (ctx.status != S3StatusOK) {
         /* error message should have been filled within response cb */
         return false;
      } else {
         Dmsg2(dbglvl, "clean_cloud_volume for %s: Unlink file %s.\n", VolumeName, part);
      }
   }
   return true;
}

/*
 * Not thread safe
 */
bool s3_driver::truncate_cloud_volume(const char *VolumeName, ilist *trunc_parts, cancel_callback *cancel_cb, POOLMEM *&err)
{
   Enter(dbglvl);

   bacula_ctx ctx(err);

   int last_index = (int)trunc_parts->last_index();
   POOLMEM *cloud_fname = get_pool_memory(PM_FNAME);
   for (int i=1; (i<=last_index); i++) {
      if (!trunc_parts->get(i)) {
         continue;
      }
      if (cancel_cb && cancel_cb->fct && cancel_cb->fct(cancel_cb->arg)) {
         Mmsg(err, _("Job cancelled.\n"));
         goto get_out;
      }
      /* don't forget to specify the volume name is the object path */
      make_cloud_filename(cloud_fname, VolumeName, i);
      Dmsg1(dbglvl, "Object to truncate: %s\n", cloud_fname);
      ctx.caller = "S3_delete_object";
      S3_delete_object(&s3ctx, cloud_fname, NULL, 0, &responseHandler, &ctx);
      if (ctx.status != S3StatusOK) {
         /* error message should have been filled within response cb */
         goto get_out;
      }
   }

get_out:
   free_pool_memory(cloud_fname);
   bfree_and_null(ctx.nextMarker);
   return (err[0] == 0);
}

void s3_driver::make_cloud_filename(POOLMEM *&filename,
        const char *VolumeName, uint32_t apart)
{
   filename[0] = 0;
   add_vol_and_part(filename, VolumeName, "part", apart);
   Dmsg1(dbglvl, "make_cloud_filename: %s\n", filename);
}

bool s3_driver::retry_put_object(S3Status status, int retry)
{
   if (S3_status_is_retryable(status)) {
      Dmsg2(dbglvl, "retry copy_cache_part_to_cloud() status=%s %d\n", S3_get_status_name(status), retry);
      bmicrosleep((max_upload_retries - retry + 1) * 3, 0); /* Wait more and more after each retry */
      return true;
   }
   return false;
}

/*
 * Copy a single cache part to the cloud
 */
bool s3_driver::copy_cache_part_to_cloud(transfer *xfer)
{
   Enter(dbglvl);
   POOLMEM *cloud_fname = get_pool_memory(PM_FNAME);
   make_cloud_filename(cloud_fname, xfer->m_volume_name, xfer->m_part);
   uint32_t retry = max_upload_retries;
   S3Status status = S3StatusOK;
   do {
      /* when the driver decide to retry, it must reset the processed size */
      xfer->inc_retry();
      xfer->reset_processed_size();
      status = put_object(xfer, xfer->m_cache_fname, cloud_fname);
      --retry;
   } while (retry_put_object(status, retry) && (retry>0));
   free_pool_memory(cloud_fname);
   return (status == S3StatusOK);
}

/*
 * Copy a single object (part) from the cloud to the cache
 */
int s3_driver::copy_cloud_part_to_cache(transfer *xfer)
{
   Enter(dbglvl);
   POOLMEM *cloud_fname = get_pool_memory(PM_FNAME);
   make_cloud_filename(cloud_fname, xfer->m_volume_name, xfer->m_part);
   int rtn = get_cloud_object(xfer, cloud_fname, xfer->m_cache_fname);
   free_pool_memory(cloud_fname);
   return rtn;
}

bool s3_driver::restore_cloud_object(transfer *xfer, const char *cloud_fname)
{
   if (glacier_item.ptr) {
      return glacier_item.ptr->restore_cloud_object(xfer,cloud_fname);
   }
   return false;
}

bool s3_driver::is_waiting_on_server(transfer *xfer)
{
   Enter(dbglvl);
   POOL_MEM cloud_fname(PM_FNAME);
   make_cloud_filename(cloud_fname.addr(), xfer->m_volume_name, xfer->m_part);
   if (glacier_item.ptr) {
      return glacier_item.ptr->is_waiting_on_server(xfer, cloud_fname.addr());
   }
   return false;
}
/*
 * NOTE: See the SD Cloud resource in stored_conf.h
*/

bool s3_driver::init(CLOUD *cloud, POOLMEM *&err)
{
   S3Status status;
   if (cloud->host_name == NULL) {
      Mmsg1(err, "Failed to initialize S3 Cloud. ERR=Hostname not set in cloud resource %s\n", cloud->hdr.name);
      return false;
   }
   if (cloud->access_key == NULL) {
      Mmsg1(err, "Failed to initialize S3 Cloud. ERR=AccessKey not set in cloud resource %s\n", cloud->hdr.name);
      return false;
   }
   if (cloud->secret_key == NULL) {
      Mmsg1(err, "Failed to initialize S3 Cloud. ERR=SecretKey not set in cloud resource %s\n", cloud->hdr.name);
      return false;
   }
   /* Setup bucket context for S3 lib */
   s3ctx.hostName = cloud->host_name;
   s3ctx.bucketName = cloud->bucket_name;
   s3ctx.protocol = (S3Protocol)cloud->protocol;
   s3ctx.uriStyle = (S3UriStyle)cloud->uri_style;
   s3ctx.accessKeyId = cloud->access_key;
   s3ctx.secretAccessKey = cloud->secret_key;
   s3ctx.authRegion = cloud->region;

   if ((status = S3_initialize("s3", S3_INIT_ALL, s3ctx.hostName)) != S3StatusOK) {
      Mmsg1(err, "Failed to initialize S3 lib. ERR=%s\n", S3_get_status_name(status));
      return false;
   }

   /*load glacier */
   if (me) {
      load_glacier_driver(me->plugin_directory);
      if (glacier_item.ptr) {
         s3_cloud_glacier_ctx glacier_ctx;
         glacier_ctx.host_name = cloud->host_name;
         glacier_ctx.bucket_name = cloud->bucket_name;
         glacier_ctx.protocol = (S3Protocol)cloud->protocol;
         glacier_ctx.uri_style = (S3UriStyle)cloud->uri_style;
         glacier_ctx.access_key = cloud->access_key;
         glacier_ctx.secret_key = cloud->secret_key;
         glacier_ctx.region = cloud->region;
         glacier_ctx.transfer_priority = cloud->transfer_priority;
         glacier_ctx.transfer_retention = cloud->transfer_retention;

         if (!glacier_item.ptr->init(&glacier_ctx, err)) {
            return false;
         }
      }
   }

   return true;
}

bool s3_driver::start_of_job(POOLMEM *&msg)
{
   if (msg) {
      Mmsg(msg, _("Using S3 cloud driver Host=%s Bucket=%s"), s3ctx.hostName, s3ctx.bucketName);
   }
   return true;
}

bool s3_driver::end_of_job(POOLMEM *&msg)
{
   return true;
}

bool s3_driver::term(POOLMEM *&msg)
{
   unload_drivers();
   S3_deinitialize();
   return true;
}

/*
 * libs3 callback for get_num_cloud_volume_parts_list()
 */
static S3Status partslistBucketCallback(
   int isTruncated,
   const char *nextMarker,
   int numObj,
   const S3ListBucketContent *object,
   int commonPrefixesCount,
   const char **commonPrefixes,
   void *callbackCtx)
{
   bacula_ctx *ctx = (bacula_ctx *)callbackCtx;

   Enter(dbglvl);
   for (int i = 0; ctx->parts && (i < numObj); i++) {
      const S3ListBucketContent *obj = &(object[i]);
      const char *ext=strstr(obj->key, "part.");
      if (obj && ext!=NULL) {
         cloud_part *part = (cloud_part*) malloc(sizeof(cloud_part));

         part->index = atoi(&(ext[5]));
         part->mtime = obj->lastModified;
         part->size  = obj->size;
         bmemzero(part->hash64, 64);
         ctx->parts->put(part->index, part);
         Dmsg1(dbglvl, "partslistBucketCallback: part.%d retrieved\n", part->index);
      }
   }

   ctx->isTruncated = isTruncated;
   if (ctx->nextMarker) {
      bfree_and_null(ctx->nextMarker);
   }
   if (isTruncated && numObj>0) {
      /* Workaround a bug with nextMarker */
      const S3ListBucketContent *obj = &(object[numObj-1]);
      ctx->nextMarker = bstrdup(obj->key);
   }

   Leave(dbglvl);
   if (ctx->cancel_cb && ctx->cancel_cb->fct && ctx->cancel_cb->fct(ctx->cancel_cb->arg)) {
      Mmsg(ctx->errMsg, _("Job cancelled.\n"));
      return S3StatusAbortedByCallback;
   }
   return S3StatusOK;
}

S3ListBucketHandler partslistBucketHandler =
{
   responseHandler,
   &partslistBucketCallback
};

bool s3_driver::get_cloud_volume_parts_list(const char* VolumeName, ilist *parts, cancel_callback *cancel_cb, POOLMEM *&err)
{
   Enter(dbglvl);

   if (!parts || strlen(VolumeName) == 0) {
      pm_strcpy(err, "Invalid argument");
      return false;
   }

   bacula_ctx ctx(err);
   ctx.cancel_cb = cancel_cb;
   ctx.parts = parts;
   ctx.isTruncated = 1; /* pass into the while loop at least once */
   ctx.caller = "S3_list_bucket";
   while (ctx.isTruncated!=0) {
      ctx.isTruncated = 0;
      S3_list_bucket(&s3ctx, VolumeName, ctx.nextMarker, NULL, 0, NULL, 0, &partslistBucketHandler, &ctx);
      Dmsg4(dbglvl, "get_cloud_volume_parts_list isTruncated=%d, nextMarker=%s, nbparts=%d, err=%s\n", ctx.isTruncated, ctx.nextMarker, ctx.parts->size(), ctx.errMsg?ctx.errMsg:"None");
      if (ctx.status != S3StatusOK) {
         pm_strcpy(err, S3Errors[ctx.status]);
         bfree_and_null(ctx.nextMarker);
         return false;
      }
   }
   bfree_and_null(ctx.nextMarker);
   return true;
}

bool s3_driver::get_one_cloud_volume_part(const char* part_path_name, ilist *parts, POOLMEM *&err)
{
   Enter(dbglvl);

   if (!parts || strlen(part_path_name) == 0) {
      pm_strcpy(err, "Invalid argument");
      return false;
   }

   bacula_ctx ctx(err);
   ctx.parts = parts;
   ctx.isTruncated = 0; /* ignore truncation in this case */
   ctx.caller = "S3_list_bucket";
   /* S3 documentation claims the parts will be returned in binary order */
   /* so part.1 < part.11 b.e. This assumed, the first part retrieved is the one we seek */
   S3_list_bucket(&s3ctx, part_path_name, ctx.nextMarker, NULL, 1, NULL, 0, &partslistBucketHandler, &ctx);
   Dmsg4(dbglvl, "get_one_cloud_volume_part isTruncated=%d, nextMarker=%s, nbparts=%d, err=%s\n", ctx.isTruncated, ctx.nextMarker, ctx.parts->size(), ctx.errMsg?ctx.errMsg:"None");
   if (ctx.status != S3StatusOK) {
      pm_strcpy(err, S3Errors[ctx.status]);
      bfree_and_null(ctx.nextMarker);
      return false;
   }

   bfree_and_null(ctx.nextMarker);
   return true;
}

/*
 * libs3 callback for get_cloud_volumes_list()
 */
static S3Status volumeslistBucketCallback(
   int isTruncated,
   const char *nextMarker,
   int numObj,
   const S3ListBucketContent *object,
   int commonPrefixesCount,
   const char **commonPrefixes,
   void *callbackCtx)
{
   bacula_ctx *ctx = (bacula_ctx *)callbackCtx;

   Enter(dbglvl);
   for (int i = 0; ctx->volumes && (i < commonPrefixesCount); i++) {
      char *cp = bstrdup(commonPrefixes[i]);
      cp[strlen(cp)-1] = 0;
      ctx->volumes->append(cp);
   }

   ctx->isTruncated = isTruncated;
   if (ctx->nextMarker) {
      bfree_and_null(ctx->nextMarker);
   }
   if (isTruncated && numObj>0) {
      /* Workaround a bug with nextMarker */
      const S3ListBucketContent *obj = &(object[numObj-1]);
      ctx->nextMarker = bstrdup(obj->key);
   }

   Leave(dbglvl);
   if (ctx->cancel_cb && ctx->cancel_cb->fct && ctx->cancel_cb->fct(ctx->cancel_cb->arg)) {
      Mmsg(ctx->errMsg, _("Job cancelled.\n"));
      return S3StatusAbortedByCallback;
   }
   return S3StatusOK;
}

S3ListBucketHandler volumeslistBucketHandler =
{
   responseHandler,
   &volumeslistBucketCallback
};

bool s3_driver::get_cloud_volumes_list(alist *volumes, cancel_callback *cancel_cb, POOLMEM *&err)
{
   Enter(dbglvl);

   if (!volumes) {
      pm_strcpy(err, "Invalid argument");
      return false;
   }

   bacula_ctx ctx(err);
   ctx.volumes = volumes;
   ctx.cancel_cb = cancel_cb;
   ctx.isTruncated = 1; /* pass into the while loop at least once */
   ctx.caller = "S3_list_bucket";
   while (ctx.isTruncated!=0) {
      ctx.isTruncated = 0;
      S3_list_bucket(&s3ctx, NULL, ctx.nextMarker, "/", 0, NULL, 0,
                     &volumeslistBucketHandler, &ctx);
      if (ctx.status != S3StatusOK) {
         break;
      }
   }
   bfree_and_null(ctx.nextMarker);
   return (err[0] == 0);
}

#endif /* HAVE_LIBS3 */
