/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#define LTC_DEPRECATED(x)
#include  <tomcrypt_test.h>

#ifdef LTC_MECC
static void s_ecc_test(void)
{
   const ltc_ecc_curve* dp;
   unsigned char buf[128];
   unsigned long len;
   ecc_key key;
   int stat;
   unsigned char data16[16] = { 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1 };

   /* We need an MPI provider for ECC */
   if (ltc_mp.name == NULL) return;

   ENSURE(ltc_ecc_curves[0].OID != NULL);

   DO(ecc_find_curve(ltc_ecc_curves[0].OID, &dp));
   DO(ecc_make_key_ex(&yarrow_prng, find_prng ("yarrow"), &key, dp));

   len = sizeof(buf);
   DO(ecc_sign_hash(data16, 16, buf, &len, &yarrow_prng, find_prng ("yarrow"), &key));
   stat = 0;
   DO(ecc_verify_hash(buf, len, data16, 16, &stat, &key));

   SHOULD_FAIL(ecc_verify_hash_rfc7518(buf, len, data16, 16, &stat, &key));

   len = sizeof(buf);
   DO(ecc_sign_hash_rfc7518(data16, 16, buf, &len, &yarrow_prng, find_prng ("yarrow"), &key));
   stat = 0;
   DO(ecc_verify_hash_rfc7518(buf, len, data16, 16, &stat, &key));

   SHOULD_FAIL(ecc_verify_hash(buf, len, data16, 16, &stat, &key));

   ecc_free(&key);
}
#endif

int deprecated_test(void)
{
#ifdef LTC_MECC
   s_ecc_test();
#endif
   return 0;
}
