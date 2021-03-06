#ifndef LOGICAL_ENCLAVE_U_H__
#define LOGICAL_ENCLAVE_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_status_t etc. */

#include "sgx_eid.h"
#include "sgx_dh.h"

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

int SGX_UBRIDGE(SGX_NOCONVENTION, Encryptusershuju, (int dataid, uint8_t* usershuju, size_t len));
void SGX_UBRIDGE(SGX_NOCONVENTION, disp, (uint8_t* pbuf, size_t len));
int SGX_UBRIDGE(SGX_NOCONVENTION, Updatefileindisk, (int dataid, uint8_t* file, size_t len));
uint32_t SGX_UBRIDGE(SGX_NOCONVENTION, TransferRequestToL, (uint8_t* request, size_t len, uint8_t* Response, size_t Reslen));
sgx_status_t SGX_UBRIDGE(SGX_NOCONVENTION, create_session_ocall, (uint32_t* sid, uint8_t* dh_msg1, uint32_t dh_msg1_size, uint32_t timeout));
sgx_status_t SGX_UBRIDGE(SGX_NOCONVENTION, exchange_report_ocall, (uint32_t sid, uint8_t* dh_msg2, uint32_t dh_msg2_size, uint8_t* dh_msg3, uint32_t dh_msg3_size, uint32_t timeout));
sgx_status_t SGX_UBRIDGE(SGX_NOCONVENTION, close_session_ocall, (uint32_t sid, uint32_t timeout));
sgx_status_t SGX_UBRIDGE(SGX_NOCONVENTION, invoke_service_ocall, (uint8_t* pse_message_req, uint32_t pse_message_req_size, uint8_t* pse_message_resp, uint32_t pse_message_resp_size, uint32_t timeout));
void SGX_UBRIDGE(SGX_CDECL, sgx_oc_cpuidex, (int cpuinfo[4], int leaf, int subleaf));
int SGX_UBRIDGE(SGX_CDECL, sgx_thread_wait_untrusted_event_ocall, (const void* self));
int SGX_UBRIDGE(SGX_CDECL, sgx_thread_set_untrusted_event_ocall, (const void* waiter));
int SGX_UBRIDGE(SGX_CDECL, sgx_thread_setwait_untrusted_events_ocall, (const void* waiter, const void* self));
int SGX_UBRIDGE(SGX_CDECL, sgx_thread_set_multiple_untrusted_events_ocall, (const void** waiters, size_t total));

sgx_status_t session_request(sgx_enclave_id_t eid, uint32_t* retval, sgx_enclave_id_t src_enclave_id, sgx_dh_msg1_t* dh_msg1, uint32_t* session_id);
sgx_status_t exchange_report(sgx_enclave_id_t eid, uint32_t* retval, sgx_enclave_id_t src_enclave_id, sgx_dh_msg2_t* dh_msg2, sgx_dh_msg3_t* dh_msg3, uint32_t session_id);
sgx_status_t Encryptuserfile(sgx_enclave_id_t eid, uint32_t* retval, uint8_t* file, size_t len, uint8_t* Entemfile, size_t outlen);
sgx_status_t GetdatatoClient(sgx_enclave_id_t eid, uint32_t* retval, int ID, uint8_t* data, size_t len, uint8_t* Enuserdata, size_t Enlen);
sgx_status_t Deblocking(sgx_enclave_id_t eid, uint32_t* retval, int tdataid);
sgx_status_t WritebackdatatoDisk(sgx_enclave_id_t eid, uint32_t* retval);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
