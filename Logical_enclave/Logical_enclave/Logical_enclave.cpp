#include "Logical_enclave_t.h"
#include <string>
#include <map>
#include <queue>
#include "sgx_tae_service.h"
#include "sgx_tseal.h"
#include "ipp/ippcp.h"
#include "sgx_trts.h"
#include "sgx_thread.h"
#include <atomic>
#define ENFILELEN 1604
sgx_key_128bit_t dh_aek;   // Session key
sgx_dh_session_t sgx_dh_session;


//uerfile数据结构
typedef struct userfile {
	sgx_mc_uuid_t mc;
	uint32_t mc_value;
	uint8_t secret[];
}uf;
uint32_t session_request(sgx_enclave_id_t src_enclave_id, sgx_dh_msg1_t *dh_msg1, uint32_t *session_id)
{


	sgx_status_t status = SGX_SUCCESS;


	//Intialize the session as a session responder
	status = sgx_dh_init_session(SGX_DH_SESSION_RESPONDER, &sgx_dh_session);
	if (SGX_SUCCESS != status)
	{
		return status;
	}

	//get a new SessionID
	*session_id = 1;

	//Generate Message1 that will be returned to Source Enclave
	status = sgx_dh_responder_gen_msg1((sgx_dh_msg1_t*)dh_msg1, &sgx_dh_session);
	return status;
}
uint32_t exchange_report(sgx_enclave_id_t src_enclave_id, sgx_dh_msg2_t *dh_msg2, sgx_dh_msg3_t *dh_msg3, uint32_t session_id)
{
	sgx_dh_session_enclave_identity_t initiator_identity;
	uint32_t rs = 0;
	memset(&dh_aek, 0, sizeof(sgx_key_128bit_t));
	do
	{
		dh_msg3->msg3_body.additional_prop_length = 0;
		//Process message 2 from source enclave and obtain message 3
		sgx_status_t se_ret = sgx_dh_responder_proc_msg2(dh_msg2, dh_msg3, &sgx_dh_session, &dh_aek, &initiator_identity);
		if (SGX_SUCCESS != se_ret)
		{
			rs = -1;
			break;
		}
		
	} while (0);
	disp(dh_aek, sizeof(sgx_aes_ctr_128bit_key_t));
	return rs;
}
//加密
uint32_t AES_Encryptcbc(uint8_t* key, size_t len, uint8_t *plaintext, size_t plen, uint8_t *Entext) {
	int size = 0;
	IppStatus re = ippStsNoErr;
	ippsAESGetSize(&size);
	IppsAESSpec *pCtx;
	pCtx = (IppsAESSpec *)malloc(size);
	memset(pCtx, 0, size);
	re = ippsAESInit(key, len, pCtx, size);
	Ipp8u piv[] = "\xff\xee\xdd\xcc\xbb\xaa\x99\x88"
		"\x77\x66\x55\x44\x33\x22\x11\x00";
	Ipp8u ctr[16];
	memcpy(ctr, piv, sizeof(ctr));
	re = ippsAESEncryptCBC(plaintext, Entext, plen, pCtx, ctr);
	ippsAESInit(0, len, pCtx, size);
	free(pCtx);
	return re;
}
//解密
uint32_t AES_Decryptcbc(uint8_t* key, size_t len, uint8_t *Entext, uint8_t *plaintext, size_t plen) {
	int size = 0;
	IppStatus re = ippStsNoErr;
	ippsAESGetSize(&size);
	IppsAESSpec *pCtx;
	pCtx = (IppsAESSpec *)malloc(size);
	memset(pCtx, 0, size);
	re = ippsAESInit(key, len, pCtx, size);
	Ipp8u piv[] = "\xff\xee\xdd\xcc\xbb\xaa\x99\x88"
		"\x77\x66\x55\x44\x33\x22\x11\x00";
	Ipp8u ctr[16];
	memcpy(ctr, piv, sizeof(ctr));
	re = ippsAESDecryptCBC(Entext, plaintext, plen, pCtx, ctr);
	ippsAESInit(0, len, pCtx, size);
	free(pCtx);
	return re;
}
std::map<int,uf*> *userfile = new std::map<int,uf*>;
std::queue<int> *FIFOqueue = new std::queue<int>;//用于保存FIFO的顺序，目前设置缓存为10000个文件
std::map<int, int> *wfilelock = new std::map<int, int>;//用于保存锁，如果是写请求就将对应文件加锁。
sgx_thread_mutex_t lock = SGX_THREAD_MUTEX_INITIALIZER;
sgx_thread_mutex_t wf_mutex = SGX_THREAD_MUTEX_INITIALIZER;//文件写锁
//sgx_thread_cond_t wc_cond = SGX_THREAD_COND_INITIALIZER;//条件锁
typedef struct Tofileenclave {
	int dataid;
	sgx_ec256_dh_shared_t userkey;
	int ac;
};
typedef struct UserRequest {
	uint32_t ID;
	uint32_t len;
	uint8_t data[16];
};
//给用户的文件解锁
uint32_t Deblocking(int tdataid) 
{
	sgx_thread_mutex_lock(&wf_mutex);
	wfilelock->erase(wfilelock->find(tdataid));
	sgx_thread_mutex_unlock(&wf_mutex);
	uint32_t re = 1;
	if (wfilelock->find(tdataid) == wfilelock->end()) re = 0;
	return re;
}
//get encrypt size 为了使要加密的数据长度为16的倍数，所以需要进行数据填充
uint32_t getEncryptdatalen(int len) {
	uint32_t size = 0;
	if (len % 16 == 0) {
		size = len;
	}
	else {
		size = len + (16 - (len % 16));
	}
	return size;
}
uint32_t FindfileTOuser(uint8_t* data, size_t len, uint8_t *Enuserdata, size_t len2);
uint32_t GetdatatoClient(int ID, uint8_t* data, size_t len, uint8_t* Enuserdata, size_t Enlen) {
	UserRequest tampR;
	Tofileenclave tampF;
	int Responsesize = getEncryptdatalen(sizeof(tampF));
	int Requestsize = getEncryptdatalen(sizeof(tampR));
	uint8_t *Entampf = new uint8_t[Responsesize];
	uint8_t *EnR=new uint8_t[Requestsize];
	uint32_t re = 0;
	tampR.ID = ID;
	tampR.len = len;
	memcpy(tampR.data,data,len);
	memset(EnR,0,Requestsize);
	memcpy(EnR,(uint8_t*)&tampR,sizeof(tampR));
	re = AES_Encryptcbc(dh_aek,sizeof(sgx_aes_ctr_128bit_key_t),EnR,Requestsize,EnR);
	TransferRequestToL(&re, EnR, Requestsize,Entampf,Responsesize);
	delete[] EnR;
	if (re == 0) {
		re=FindfileTOuser(Entampf,Responsesize,Enuserdata,Enlen);
	}
	delete[] Entampf;
	return re;
}
//增加计数器的值
uint32_t UpdateCount(sgx_mc_uuid_t *mc,uint32_t *tmc_value) {
	int busy_retry_times = 2;
	sgx_status_t ret = SGX_SUCCESS;
	do {
		ret = sgx_create_pse_session();
	} while (ret == SGX_ERROR_BUSY && busy_retry_times--);
	if (ret != SGX_SUCCESS) {
		return ret;
	}
	uint32_t mc_value = 0;
	ret = sgx_read_monotonic_counter(mc, &mc_value);
	if (mc_value != *tmc_value)
	{
		return ret;
	}
	ret = sgx_increment_monotonic_counter(mc, tmc_value);
	if (ret != SGX_SUCCESS)
	{
		return ret;
	}
	ret = sgx_close_pse_session();
	return ret;
}
uint32_t FindfileTOuser(uint8_t* data, size_t len, uint8_t *Enuserdata, size_t len2) {
	int re = 0;
	uf *useruf;
	Tofileenclave tamp;
	sgx_status_t ret = SGX_SUCCESS;
	
	uint8_t *getendatafromenclave1 = new uint8_t[len];
	re = AES_Decryptcbc(dh_aek, sizeof(sgx_aes_ctr_128bit_key_t),data,getendatafromenclave1,len);
	if (re != 0) return re;
	memcpy(&tamp, getendatafromenclave1, sizeof(Tofileenclave));
	delete[] getendatafromenclave1;
	//判断用户是否为写请求，若为写则加锁。
	sgx_thread_mutex_lock(&wf_mutex);
	int isW=wfilelock->count(tamp.dataid);
	if (isW == 0) {
		if (tamp.ac == 2) {
			wfilelock->insert(std::pair<int,int>(tamp.dataid,1));
		}
	}
	else
	{
		sgx_thread_mutex_unlock(&wf_mutex);
		return -3;
	}
	sgx_thread_mutex_unlock(&wf_mutex);
	if (userfile->find(tamp.dataid) == userfile->end()) {
		uint8_t *enfile = new uint8_t[ENFILELEN];
		Encryptusershuju(&re, tamp.dataid, enfile, ENFILELEN);
		if (re != 0) return re;
		useruf = (uf*)malloc(ENFILELEN-560);
		uint32_t datalen = ENFILELEN - 560;
		ret = sgx_unseal_data((sgx_sealed_data_t*)enfile,NULL,0,(uint8_t*)useruf,&datalen);
		delete[] enfile;
		if (ret != SGX_SUCCESS) {
			return -1;
		}
		//计数器++
		UpdateCount(&useruf->mc,&useruf->mc_value);
		//开线程异步写回disk
		//Updatefileindisk(&re, tamp.dataid,(uint8_t*)useruf,ENFILELEN);
		//int tmpsize = FIFOqueue->size();
		
		if (FIFOqueue->size() <= 10000) {
			
			FIFOqueue->push(tamp.dataid);
			userfile->insert(std::pair<int, uf*>(tamp.dataid, useruf));
		}
		else
		{	
			int topid = FIFOqueue->front();		
			uint8_t updatefile[ENFILELEN];		
			memcpy(updatefile,userfile->find(topid)->second,ENFILELEN);
			int re = 0;
			Updatefileindisk(&re,topid,updatefile,ENFILELEN);
			if (re == 0) {
				FIFOqueue->pop();	
				userfile->erase(topid);
				userfile->insert(std::pair<int, uf*>(tamp.dataid, useruf));
			}
			else
			{
				return -1;
			}
		}
		re = AES_Encryptcbc(tamp.userkey.s, SGX_ECP256_KEY_SIZE, useruf->secret, 1024, Enuserdata);
	}
	else 
	{
		//什么时候往disk写是个问题，目前都在map中更新。
		UpdateCount(&userfile->find(tamp.dataid)->second->mc,&userfile->find(tamp.dataid)->second->mc_value);
		//开线程异步写回disk
		//Updatefileindisk(&re, tamp.dataid, (uint8_t*)userfile->find(tamp.dataid)->second, ENFILELEN);
		re = AES_Encryptcbc(tamp.userkey.s, SGX_ECP256_KEY_SIZE, userfile->find(tamp.dataid)->second->secret, 1024, Enuserdata);
	}
	return re;
}
//用户file加密并绑定计数器
uint32_t Encryptuserfile(uint8_t* file, size_t len,uint8_t *Entemfile,size_t outlen) {
	sgx_status_t ret = SGX_SUCCESS;
	uf *temuf = (uf*)malloc(sizeof(uf)+len);
	memset(temuf, 0, sizeof(uf) + len);
	int busy_retry_times = 2;
	uint32_t size = sgx_calc_sealed_data_size(0, sizeof(uf) + len);
	do {
		ret = sgx_create_pse_session();
	} while (ret == SGX_ERROR_BUSY && busy_retry_times--);
	if (ret != SGX_SUCCESS) {
		return -1;
	}
	ret = sgx_create_monotonic_counter(&temuf->mc, &temuf->mc_value);
	if (ret != SGX_SUCCESS)
	{
		return -1;
	}
	memcpy(&temuf->secret, file, len);
	uint32_t datalen = sizeof(uf) + len;
	ret = sgx_seal_data(0, NULL, datalen, (uint8_t*)temuf, outlen, (sgx_sealed_data_t*)Entemfile);
	ret = sgx_close_pse_session();
	if (ret != SGX_SUCCESS) {
		return -1;
	}
	return ret;
}
////程序结束时将map内所有数据写回disk
uint32_t WritebackdatatoDisk() {
	sgx_thread_mutex_lock(&lock);
	uint32_t re = 0;
	std::map<int,uf*>::iterator tamuf;
	for (tamuf = userfile->begin(); tamuf != userfile->end(); tamuf++) {
		Updatefileindisk((int*)&re, tamuf->first, (uint8_t*)tamuf->second, ENFILELEN);
	}
	sgx_thread_mutex_unlock(&lock);
	return re;
}