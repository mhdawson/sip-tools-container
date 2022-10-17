#include <assert.h>
#include <node_api.h>
#include <pjsua-lib/pjsua.h>

#define MAX_SMS_MESSAGE_SIZE 160
#define BUFFER_MAX 1000

#define DECLARE_NAPI_METHOD(name, func)                                        \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

#define CHECK(expr, message) \
  { \
    if ((expr) == 0) { \
      napi_throw_error((env), NULL, (message)); \
      return NULL; \
    } \
  }

#define CHECK_VOID(expr, message) \
  { \
    if ((expr) == 0) { \
      napi_throw_error((env), NULL, (message)); \
    } \
  }

typedef struct {
  napi_threadsafe_function tsfn;
  napi_ref sms_cb_ref;
  pjsua_acc_id account;
} AddonData;

void cleanup_addon_data(napi_env env,
                        void* finalize_data,
                        void* finalize_hint) {

  AddonData* addon_data = (AddonData*) finalize_data;
  napi_delete_reference(env, addon_data-> sms_cb_ref); 
  free(addon_data);
}

// work around as pjsua callback has no way to pass
// data
AddonData* global_addon_data;

static void on_sms(pjsua_call_id call_id,
	    const pj_str_t *from,
	    const pj_str_t *to,
	    const pj_str_t *contact,
	    const pj_str_t *mime_type,
	    const pj_str_t *body) {
  char* message = malloc(MAX_SMS_MESSAGE_SIZE+1);
  strncpy(message, body->ptr, MAX_SMS_MESSAGE_SIZE);
  message[MAX_SMS_MESSAGE_SIZE] = 0;
  napi_call_threadsafe_function(global_addon_data->tsfn, message, napi_tsfn_blocking);
};

static napi_value SetOnSMSReceived(napi_env env, const napi_callback_info info) {
  napi_status status;

  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  CHECK(status == napi_ok, "failed to get onSMS callback");

  AddonData* addon_data;
  status = napi_get_instance_data(env, (void**) &addon_data);
  CHECK(status == napi_ok, "failed to get addon data");

  status = napi_create_reference(env,
                        args[0],
                        1,
			&addon_data->sms_cb_ref);
  CHECK(status == napi_ok, "failed to store onSMS callback");

  return NULL;
}

static napi_value Stop(napi_env env, const napi_callback_info info) {
  pjsua_destroy();
  AddonData* addon_data;
  napi_status status = napi_get_instance_data(env, (void**) &addon_data);
  CHECK(status == napi_ok, "failed to get addon data");
  status = napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_abort);
  CHECK(status == napi_ok, "failed to release tsfn");
  return NULL;
}

static void SMSReceived(napi_env env, napi_value js_cb, void* context, void* data) {
  napi_status status;
  AddonData* addon_data = (AddonData*) context;

  napi_value argv[1];
  status = napi_create_string_utf8(env, (char*) data, NAPI_AUTO_LENGTH, argv);
  free(data);
  CHECK_VOID(status == napi_ok, "failed to create string for sms");

  napi_value global;
  status = napi_get_global(env, &global);
  CHECK_VOID(status == napi_ok, "failed to get global object");

  napi_value result;
  napi_value sms_cb;
  status = napi_get_reference_value(env, addon_data->sms_cb_ref, &sms_cb);
  CHECK_VOID(status == napi_ok, "failed to get cb from ref");
  status = napi_call_function(env, global, sms_cb, 1, argv, &result);
  CHECK_VOID(status == napi_ok, "failed to call sms cb");
}

static napi_value SetAccountInfo(napi_env env, const napi_callback_info info) {
  pj_status_t result;

  napi_value args[4];
  size_t argc = 4;
  napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  CHECK(status == napi_ok, "failed to get cb info in SetAccountInfo");

  if (argc <4) {
    napi_throw_error(env, NULL, "missing arguments"); 
    return NULL; 
  }

  AddonData* addon_data;
  status = napi_get_instance_data(env, (void**) &addon_data);
  CHECK(status == napi_ok, "failed to get addon data");

  char req_URI[BUFFER_MAX+1];
  char from_URI[BUFFER_MAX+1];
  char userid[BUFFER_MAX+1];
  char password[BUFFER_MAX+1];
  size_t length;

  status = napi_get_value_string_utf8(env,
                                      args[0],
                                      from_URI, 
                                      BUFFER_MAX, 
                                      &length);
  CHECK(status == napi_ok, "failed to get from_URI string");

  status = napi_get_value_string_utf8(env,
                                      args[1],
                                      req_URI, 
                                      BUFFER_MAX, 
                                      &length);
  CHECK(status == napi_ok, "failed to get reg_URI string");


  status = napi_get_value_string_utf8(env,
                                      args[2],
                                      userid, 
                                      BUFFER_MAX, 
                                      &length);
  CHECK(status == napi_ok, "failed to get userid string");

  status = napi_get_value_string_utf8(env,
                                      args[3],
                                      password, 
                                      BUFFER_MAX, 
                                      &length);
  CHECK(status == napi_ok, "failed to get password string");

  pj_log_set_level(0);

  result = pjsua_create();
  CHECK(result == PJ_SUCCESS, "Failed to create pjsua");

  pjsua_config cfg;
  pjsua_config_default(&cfg);
  cfg.cb.on_pager = &on_sms;

  pjsua_logging_config log_cfg;
  pjsua_logging_config_default(&log_cfg);
  log_cfg.console_level = 0;
  log_cfg.level = 0;
  log_cfg.msg_logging = 0;

  result = pjsua_init(&cfg, &log_cfg, NULL);
  CHECK(result == PJ_SUCCESS, "pjsua_init failed");

  pjsua_transport_config trans_cfg;
  pjsua_transport_config_default(&trans_cfg);
  trans_cfg.port = 5060;
  result = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &trans_cfg, NULL);
  CHECK(result == PJ_SUCCESS, "transport creation failed");

  result = pjsua_start();
  CHECK(result == PJ_SUCCESS, "pjsua failed to start");

  pjsua_acc_config account_cfg;
  pjsua_acc_config_default(&account_cfg);
  account_cfg.id = pj_str(from_URI);
  account_cfg.reg_uri = pj_str(req_URI);
  account_cfg.cred_count = 1;
  account_cfg.cred_info[0].realm = pj_str("*");
  account_cfg.cred_info[0].scheme = pj_str("digest");
  account_cfg.cred_info[0].username = pj_str(userid);
  account_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
  account_cfg.cred_info[0].data = pj_str(password);
  result = pjsua_acc_add(&account_cfg, PJ_TRUE, &addon_data->account);
  CHECK(result == PJ_SUCCESS, "account add failed");

  napi_value work_name;
  status = napi_create_string_utf8(env,
                          "pjsua wrapper",
                          NAPI_AUTO_LENGTH,
                          &work_name);
  CHECK(status == napi_ok, "failed to create work name string");

  status =  napi_create_threadsafe_function(env,
                                 NULL,
                                 NULL,
                                 work_name,
                                 0,
                                 1,
                                 NULL,
                                 NULL,
                                 (void*) addon_data,
                                 SMSReceived,
                                 &addon_data->tsfn);
  CHECK(status == napi_ok, "failed to create threadsafe function");

  return NULL;
}


static napi_value SendSMS(napi_env env, const napi_callback_info info) {
  napi_value args[2];
  size_t argc = 2;
  napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  CHECK(status == napi_ok, "failed to get cb info in SendSMS");

  AddonData* addon_data;
  status = napi_get_instance_data(env, (void**) &addon_data);
  CHECK(status == napi_ok, "failed to get addon data");

  char to_buffer[1000];
  char message_buffer[1000];
  size_t length;
  status = napi_get_value_string_utf8(env,
                                      args[0],
                                      to_buffer, 
                                      1000,
                                      &length);
  CHECK(status == napi_ok, "failed to get to string");
  pj_str_t to = pj_str(to_buffer);

  status = napi_get_value_string_utf8(env,
                                      args[1],
                                      message_buffer,
                                      1000,
                                      &length);
  CHECK(status == napi_ok, "failed to get message string");
  pj_str_t body = pj_str(message_buffer);

  pj_status_t result;
  result = pjsua_im_send(addon_data->account,
		&to,
		NULL,
		&body,
		NULL,
		NULL);
  CHECK(result == PJ_SUCCESS, "failed to send sms");

  return NULL;
}



static napi_value Init(napi_env env, napi_value exports) {
  napi_status status;

  AddonData* addon_data = malloc(sizeof(AddonData));
  global_addon_data = addon_data; // work around for pjsua limitation

  napi_set_instance_data(env,
		         (void*) addon_data,
			 cleanup_addon_data,
			 NULL);

  napi_property_descriptor properties[] = {
      DECLARE_NAPI_METHOD("SetAccountInfo", SetAccountInfo),
      DECLARE_NAPI_METHOD("SetOnSMSReceived", SetOnSMSReceived),
      DECLARE_NAPI_METHOD("SendSMS", SendSMS),
      DECLARE_NAPI_METHOD("Stop", Stop),
  };
  status = napi_define_properties(env, exports, 4, properties);
  CHECK(status == napi_ok, "failed to define properties");
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
