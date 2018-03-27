/**********************************************************************
 * Copyright (c) 2017 Andrew Poelstra                                 *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef SECP256K1_MODULE_BULLETPROOF_TESTS
#define SECP256K1_MODULE_BULLETPROOF_TESTS

#include <string.h>

#include "group.h"
#include "scalar.h"
#include "testrand.h"
#include "util.h"

#include "include/secp256k1_bulletproof.h"

/* Alternate generator used for some tests */
static const secp256k1_ge secp256k1_ge_const_g2 = SECP256K1_GE_CONST(
    0, 0, 0, 0, 0, 0, 0, 1,
    0x4218f20aUL, 0xe6c646b3UL, 0x63db6860UL, 0x5822fb14UL,
    0x264ca8d2UL, 0x587fdd6fUL, 0xbc750d58UL, 0x7e76a7eeUL
);

static void test_bulletproof_api(void) {
    secp256k1_context *none = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_context *sign = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_context *vrfy = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    secp256k1_context *both = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    secp256k1_scratch *scratch = secp256k1_scratch_space_create(ctx, 1024 * 1024);
    secp256k1_generator altgen;
    secp256k1_bulletproof_generators *gens;
    secp256k1_pedersen_commitment pcommit[4];
    const secp256k1_pedersen_commitment *pcommit_arr[1];
    unsigned char proof[2000];
    const unsigned char *proof_ptr = proof;
    const unsigned char blind[32] = "   i am not a blinding factor   ";
    const unsigned char *blind_ptr[4];
    size_t blindlen = sizeof(blind);
    size_t plen = sizeof(proof);
    uint64_t value = 1234;
    int32_t ecount = 0;

    blind_ptr[0] = blind;
    blind_ptr[1] = blind;
    blind_ptr[2] = blind;
    blind_ptr[3] = blind;
    pcommit_arr[0] = pcommit;

    secp256k1_context_set_error_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(vrfy, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(both, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(vrfy, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(both, counting_illegal_callback_fn, &ecount);

    CHECK(secp256k1_generator_generate(both, &altgen, blind) != 0);
    CHECK(secp256k1_pedersen_commit(both, &pcommit[0], blind, value, &altgen) != 0);
    CHECK(secp256k1_pedersen_commit(both, &pcommit[1], blind, value, &altgen) != 0);
    CHECK(secp256k1_pedersen_commit(both, &pcommit[2], blind, value, &altgen) != 0);
    CHECK(secp256k1_pedersen_commit(both, &pcommit[3], blind, value, &altgen) != 0);

    /* generators */
    gens = secp256k1_bulletproof_generators_create(none, 256);

    /* rangeproof_prove */
    CHECK(ecount == 0);
    CHECK(secp256k1_bulletproof_rangeproof_prove(none, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_bulletproof_rangeproof_prove(sign, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_bulletproof_rangeproof_prove(vrfy, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 1);
    CHECK(ecount == 3);
    plen = 2000;
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 2, &altgen, 64, blind, NULL, 0) == 1);
    CHECK(ecount == 3);
    plen = 2000;
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 4, &altgen, 64, blind, NULL, 0) == 0); /* too few gens */
    CHECK(ecount == 4);

    CHECK(secp256k1_bulletproof_rangeproof_prove(both, NULL, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, NULL, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 6);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, NULL, &plen, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 7);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, NULL, &value, blind_ptr, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 8);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, NULL, 1, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 9);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 0, &altgen, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 10);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, NULL, 64, blind, NULL, 0) == 0);
    CHECK(ecount == 11);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 0, blind, NULL, 0) == 0);
    CHECK(ecount == 12);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 65, blind, NULL, 0) == 0);
    CHECK(ecount == 13);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, -1, blind, NULL, 0) == 0);
    CHECK(ecount == 14);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, NULL, NULL, 0) == 0);
    CHECK(ecount == 15);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, blind, 0) == 1);
    CHECK(ecount == 15);
    CHECK(secp256k1_bulletproof_rangeproof_prove(both, scratch, gens, proof, &plen, &value, blind_ptr, 1, &altgen, 64, blind, blind, 32) == 1);
    CHECK(ecount == 15);

    /* rangeproof_verify */
    CHECK(secp256k1_bulletproof_rangeproof_verify(none, scratch, gens, proof, plen, pcommit, 1, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 16);
    CHECK(secp256k1_bulletproof_rangeproof_verify(sign, scratch, gens, proof, plen, pcommit, 1, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(vrfy, scratch, gens, proof, plen, pcommit, 1, 64, &altgen, blind, 32) == 1);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, pcommit, 1, 64, &altgen, blind, 32) == 1);
    CHECK(ecount == 17);

    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, pcommit, 1, 63, &altgen, blind, 32) == 0);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen - 1, pcommit, 1, 63, &altgen, blind, 32) == 0);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, 0, pcommit, 1, 63, &altgen, blind, 32) == 0);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, pcommit, 1, 64, &altgen, blind, 31) == 0);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, pcommit, 1, 64, &altgen, NULL, 0) == 0);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, pcommit, 2, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 17);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, pcommit, 4, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 18);

    CHECK(secp256k1_bulletproof_rangeproof_verify(both, NULL, gens, proof, plen, pcommit, 1, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 19);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, NULL, proof, plen, pcommit, 1, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 20);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, NULL, plen, pcommit, 1, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 21);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, NULL, 1, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 22);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, NULL, 0, 64, &altgen, blind, 32) == 0);
    CHECK(ecount == 23);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, NULL, 0, 65, &altgen, blind, 32) == 0);
    CHECK(ecount == 24);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, NULL, 0, 0, &altgen, blind, 32) == 0);
    CHECK(ecount == 25);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, NULL, 0, 64, NULL, blind, 32) == 0);
    CHECK(ecount == 26);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, NULL, 0, 64, &altgen, NULL, 32) == 0);
    CHECK(ecount == 27);
    CHECK(secp256k1_bulletproof_rangeproof_verify(both, scratch, gens, proof, plen, NULL, 0, 64, &altgen, blind, 0) == 0);
    CHECK(ecount == 28);

    /* verify_multi */
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(none, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 29);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(sign, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 30);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(vrfy, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 1);
    CHECK(ecount == 30);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 1);
    CHECK(ecount == 30);

    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, NULL, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 31);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, NULL, &proof_ptr, 1, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 32);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, NULL, 1, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 33);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 0, plen, pcommit_arr, 1, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 34);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, NULL, 1, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 35);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 64, NULL, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 36);

    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 0, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 36);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 65, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 36);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 63, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 36);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 1, 0, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 36);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 2, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 36);
    CHECK(secp256k1_bulletproof_rangeproof_verify_multi(both, scratch, gens, &proof_ptr, 1, plen, pcommit_arr, 4, 64, &altgen, blind_ptr, &blindlen) == 0);
    CHECK(ecount == 37);

    secp256k1_bulletproof_generators_destroy(none, gens);
    secp256k1_scratch_destroy(scratch);
    secp256k1_context_destroy(none);
    secp256k1_context_destroy(sign);
    secp256k1_context_destroy(vrfy);
    secp256k1_context_destroy(both);
}

#define MAX_WIDTH (1ul << 20)
typedef struct {
    const secp256k1_scalar *a;
    const secp256k1_scalar *b;
    const secp256k1_ge *g;
    const secp256k1_ge *h;
    size_t n;
} test_bulletproof_ecmult_context;

static int test_bulletproof_ecmult_callback(secp256k1_scalar *sc, secp256k1_ge *pt, size_t idx, void *data) {
    test_bulletproof_ecmult_context *ecctx = (test_bulletproof_ecmult_context *) data;
    if (idx < ecctx->n) {
        *sc = ecctx->a[idx];
        *pt = ecctx->g[idx];
    } else {
        VERIFY_CHECK(idx < 2*ecctx->n);
        *sc = ecctx->b[idx - ecctx->n];
        *pt = ecctx->h[idx - ecctx->n];
    }
    return 1;
}

typedef struct {
    secp256k1_scalar offs;
    secp256k1_scalar ext_sc;
    secp256k1_scalar skew_sc;
    secp256k1_ge ext_pt;
    secp256k1_ge p;
    size_t n;
    int parity;
} test_bulletproof_offset_context;

static int test_bulletproof_offset_vfy_callback(secp256k1_scalar *sc, secp256k1_ge *pt, secp256k1_scalar *randomizer, size_t idx, void *data) {
    test_bulletproof_offset_context *ecctx = (test_bulletproof_offset_context *) data;
    secp256k1_scalar_set_int(&ecctx->offs, 1);
    if (idx < 2 * ecctx->n) {
        secp256k1_scalar idxsc;
        secp256k1_scalar_set_int(&idxsc, idx);
        secp256k1_scalar_mul(sc, &ecctx->skew_sc, &idxsc);
    } else {
        if (ecctx->parity) {
            *sc = ecctx->ext_sc;
            *pt = ecctx->ext_pt;
        } else {
            secp256k1_scalar_set_int(sc, 1);
            *pt = ecctx->p;
        }
    }
    secp256k1_scalar_mul(sc, sc, randomizer);
    ecctx->parity = !ecctx->parity;
    return 1;
}

typedef struct {
    const secp256k1_scalar *a_arr;
    const secp256k1_scalar *b_arr;
} secp256k1_bulletproof_ip_test_abgh_data;


static int secp256k1_bulletproof_ip_test_abgh_callback(secp256k1_scalar *sc, secp256k1_ge *pt, size_t idx, void *data) {
    secp256k1_bulletproof_ip_test_abgh_data *cbctx = (secp256k1_bulletproof_ip_test_abgh_data *) data;
    const int is_g = idx % 2 == 0;

    (void) pt;
    if (is_g) {
        *sc = cbctx->a_arr[idx / 2];
    } else {
        *sc = cbctx->b_arr[idx / 2];
    }
    return 1;
}

void test_bulletproof_inner_product(size_t n, const secp256k1_ge *gens, const size_t n_gens) {
    const secp256k1_scalar zero = SECP256K1_SCALAR_CONST(0,0,0,0,0,0,0,0);
    secp256k1_gej pj;
    secp256k1_gej tmpj, tmpj2;
    secp256k1_scalar *a_arr = (secp256k1_scalar *)checked_malloc(&ctx->error_callback, n * sizeof(*a_arr));
    secp256k1_scalar *b_arr = (secp256k1_scalar *)checked_malloc(&ctx->error_callback, n * sizeof(*b_arr));
    unsigned char commit[32] = "hash of P, c, etc. all that jazz";
    secp256k1_scalar one;
    size_t j;
    test_bulletproof_offset_context offs_ctx;
    secp256k1_bulletproof_ip_test_abgh_data abgh_data;
    secp256k1_bulletproof_innerproduct_context innp_ctx;
    unsigned char proof[2000];
    size_t plen = sizeof(proof);

    secp256k1_scratch *scratch = secp256k1_scratch_space_create(ctx, 100000 + 256 * (2 * n + 2));

    for (j = 0; j < n; j++) {
        random_scalar_order(&a_arr[j]);
        random_scalar_order(&b_arr[j]);
    }

    abgh_data.a_arr = a_arr;
    abgh_data.b_arr = b_arr;

    random_group_element_test(&offs_ctx.ext_pt);
    random_scalar_order(&offs_ctx.ext_sc);
    secp256k1_scalar_clear(&offs_ctx.skew_sc);
    offs_ctx.n = n;

    secp256k1_scalar_set_int(&one, 1);
    CHECK(secp256k1_bulletproof_inner_product_prove_impl(&ctx->ecmult_ctx, scratch, proof, &plen, gens, n_gens, &one, n, secp256k1_bulletproof_ip_test_abgh_callback, (void *) &abgh_data, commit) == 1);

    innp_ctx.proof = proof;
    memcpy(innp_ctx.commit, commit, 32);
    secp256k1_scalar_set_int(&innp_ctx.yinv, 1);
    innp_ctx.n_extra_rangeproof_points = 1;
    innp_ctx.rangeproof_cb = test_bulletproof_offset_vfy_callback;
    innp_ctx.rangeproof_cb_data = (void *) &offs_ctx;

    /* Manually do the multiexp to obtain the point P which commits to the inner product.
     * The prover never computes this because it is implicit in the range/circuit proofs. */
    {
        test_bulletproof_ecmult_context ecmult_data;
        ecmult_data.n = n;
        ecmult_data.a = a_arr;
        ecmult_data.b = b_arr;
        ecmult_data.g = gens;
        ecmult_data.h = gens + n_gens/2;
        CHECK(secp256k1_ecmult_multi_var(&ctx->ecmult_ctx, scratch, &pj, &zero, test_bulletproof_ecmult_callback, (void*) &ecmult_data, 2 * n));
        secp256k1_ge_set_gej(&offs_ctx.p, &pj);
    }

    /* Check proof with no offsets or other baubles */
    offs_ctx.parity = 0;
    secp256k1_scalar_clear(&innp_ctx.p_offs);
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, &innp_ctx, 1, plen) == 1);

    /* skew P by a random amount and instruct the verifier to offset it */
    random_scalar_order(&innp_ctx.p_offs);
    secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &tmpj, &innp_ctx.p_offs);
    secp256k1_gej_add_var(&pj, &pj, &tmpj, NULL);
    secp256k1_ge_set_gej(&offs_ctx.p, &pj);

    /* wrong p_offs should fail */
    offs_ctx.parity = 0;
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, &innp_ctx, 1, plen) == 0);

    secp256k1_scalar_negate(&innp_ctx.p_offs, &innp_ctx.p_offs);

    offs_ctx.parity = 0;
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, &innp_ctx, 1, plen) == 1);
    /* check that verification did not trash anything */
    offs_ctx.parity = 0;
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, &innp_ctx, 1, plen) == 1);
    /* check that adding a no-op rangeproof skew function doesn't break anything */
    offs_ctx.parity = 0;
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, &innp_ctx, 1, plen) == 1);

    /* Offset P by some random point and then try to undo this in the verification */
    secp256k1_gej_set_ge(&tmpj2, &offs_ctx.ext_pt);
    secp256k1_ecmult(&ctx->ecmult_ctx, &tmpj, &tmpj2, &offs_ctx.ext_sc, &zero);
    secp256k1_gej_neg(&tmpj, &tmpj);
    secp256k1_gej_add_ge_var(&tmpj, &tmpj, &offs_ctx.p, NULL);
    secp256k1_ge_set_gej(&offs_ctx.p, &tmpj);
    offs_ctx.parity = 0;
    innp_ctx.n_extra_rangeproof_points = 2;
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, &innp_ctx, 1, plen) == 1);

    /* Offset each basis by some random point and try to undo this in the verification */
    secp256k1_gej_set_infinity(&tmpj2);
    for (j = 0; j < n; j++) {
        size_t k;
        /* Offset by k-times the kth G basis and (k+n)-times the kth H basis */
        for (k = 0; k < j; k++) {
            secp256k1_gej_add_ge_var(&tmpj2, &tmpj2, &gens[j], NULL);
            secp256k1_gej_add_ge_var(&tmpj2, &tmpj2, &gens[j + n_gens/2], NULL);
        }
        for (k = 0; k < n; k++) {
            secp256k1_gej_add_ge_var(&tmpj2, &tmpj2, &gens[j + n_gens/2], NULL);
        }
    }
    random_scalar_order(&offs_ctx.skew_sc);
    secp256k1_ecmult(&ctx->ecmult_ctx, &tmpj, &tmpj2, &offs_ctx.skew_sc, &zero);
    secp256k1_gej_add_ge_var(&tmpj, &tmpj, &offs_ctx.p, NULL);
    secp256k1_ge_set_gej(&offs_ctx.p, &tmpj);
    secp256k1_scalar_negate(&offs_ctx.skew_sc, &offs_ctx.skew_sc);

    offs_ctx.parity = 0;
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, &innp_ctx, 1, plen) == 1);

    /* Try to validate the same proof twice */
{
    test_bulletproof_offset_context offs_ctxs[2];
    secp256k1_bulletproof_innerproduct_context innp_ctxs[2];
    offs_ctx.parity = 1;  /* set parity to 1 so the common point will be returned first, as required by the multi-proof verifier */
    memcpy(&innp_ctxs[0], &innp_ctx, sizeof(innp_ctx));
    memcpy(&innp_ctxs[1], &innp_ctx, sizeof(innp_ctx));
    memcpy(&offs_ctxs[0], &offs_ctx, sizeof(offs_ctx));
    memcpy(&offs_ctxs[1], &offs_ctx, sizeof(offs_ctx));
    innp_ctxs[0].rangeproof_cb_data = (void *)&offs_ctxs[0];
    innp_ctxs[1].rangeproof_cb_data = (void *)&offs_ctxs[1];
    CHECK(secp256k1_bulletproof_inner_product_verify_impl(&ctx->ecmult_ctx, scratch, gens, n_gens, n, innp_ctxs, 2, plen) == 1);
}

    free(a_arr);
    free(b_arr);
    secp256k1_scratch_destroy(scratch);
}

void test_bulletproof_rangeproof(size_t nbits, size_t expected_size, const secp256k1_ge *gens, const size_t n_gens) {
    secp256k1_scalar blind;
    unsigned char proof[1024];
    const unsigned char *proof_ptr[2];
    size_t plen = sizeof(proof);
    uint64_t v = 123456;
    secp256k1_gej commitj;
    secp256k1_ge commitp;
    const secp256k1_ge *commitp_ptr[2];
    secp256k1_ge genp[2];
    secp256k1_scalar vs;
    secp256k1_gej altgen;
    unsigned char nonce[32] = "my kingdom for some randomness!!";

    secp256k1_scratch *scratch = secp256k1_scratch_space_create(ctx, 10000000);

    if (v >> nbits > 0) {
        v = 0;
    }

    proof_ptr[0] = proof_ptr[1] = proof;

    secp256k1_gej_set_ge(&altgen, &secp256k1_ge_const_g2);
    random_scalar_order(&blind);
    secp256k1_scalar_set_u64(&vs, v);
    secp256k1_ecmult(&ctx->ecmult_ctx, &commitj, &altgen, &vs, &blind);
    secp256k1_ge_set_gej(&commitp, &commitj);
    commitp_ptr[0] = commitp_ptr[1] = &commitp;

    genp[0] = genp[1] = secp256k1_ge_const_g2;

    CHECK(secp256k1_bulletproof_rangeproof_prove_impl(&ctx->ecmult_gen_ctx, &ctx->ecmult_ctx, scratch, proof, &plen, nbits, &v, &blind, &commitp, 1, &secp256k1_ge_const_g2, gens, n_gens, nonce, NULL, 0) == 1);
    CHECK(plen == expected_size);
    /* Verify once */
    CHECK(secp256k1_bulletproof_rangeproof_verify_impl(&ctx->ecmult_ctx, scratch, proof_ptr, 1, plen, nbits, commitp_ptr, 1, genp, gens, n_gens, NULL, 0) == 1);
    /* Verify twice at once to test batch validation */
    CHECK(secp256k1_bulletproof_rangeproof_verify_impl(&ctx->ecmult_ctx, scratch, proof_ptr, 2, plen, nbits, commitp_ptr, 1, genp, gens, n_gens, NULL, 0) == 1);

    secp256k1_scratch_destroy(scratch);
}

void test_bulletproof_rangeproof_aggregate(size_t nbits, size_t n_commits, size_t expected_size, const secp256k1_ge *gens, const size_t n_gens) {
    unsigned char proof[1024];
    const unsigned char *proof_ptr = proof;
    size_t plen = sizeof(proof);
    secp256k1_scalar *blind = (secp256k1_scalar *)checked_malloc(&ctx->error_callback, n_commits * sizeof(*blind));
    uint64_t *v = (uint64_t *)checked_malloc(&ctx->error_callback, n_commits * sizeof(*v));
    secp256k1_ge *commitp = (secp256k1_ge *)checked_malloc(&ctx->error_callback, n_commits * sizeof(*commitp));
    const secp256k1_ge *constptr = commitp;
    secp256k1_ge genp;
    unsigned char commit[32] = {0};
    unsigned char nonce[32] = "mary, mary quite contrary how do";
    size_t i;

    secp256k1_scratch *scratch = secp256k1_scratch_space_create(ctx, 10000000);

    genp = secp256k1_ge_const_g2;
    for (i = 0; i < n_commits; i++) {
        secp256k1_scalar vs;
        secp256k1_gej commitj;
        secp256k1_gej genpj;

        v[i] = 223 * i; /* dice-roll random # */
        if (v[i] >> nbits > 0) {
            v[i] = 0;
        }
        secp256k1_scalar_set_u64(&vs, v[i]);
        random_scalar_order(&blind[i]);
        secp256k1_gej_set_ge(&genpj, &genp);
        secp256k1_ecmult(&ctx->ecmult_ctx, &commitj, &genpj, &vs, &blind[i]);
        secp256k1_ge_set_gej(&commitp[i], &commitj);

        secp256k1_bulletproof_update_commit(commit, &commitp[i], &genp);
    }

    CHECK(secp256k1_bulletproof_rangeproof_prove_impl(&ctx->ecmult_gen_ctx, &ctx->ecmult_ctx, scratch, proof, &plen, nbits, v, blind, commitp, n_commits, &genp, gens, n_gens, nonce, NULL, 0) == 1);
    CHECK(plen == expected_size);
    CHECK(secp256k1_bulletproof_rangeproof_verify_impl(&ctx->ecmult_ctx, scratch, &proof_ptr, 1, plen, nbits, &constptr, n_commits, &genp, gens, n_gens, NULL, 0) == 1);

    secp256k1_scratch_destroy(scratch);
    free(commitp);
    free(v);
    free(blind);
}

void run_bulletproof_tests(void) {
    size_t i;

    /* Make a ton of generators */
    size_t n_gens = 32768;
    secp256k1_ge *gens = (secp256k1_ge *)checked_malloc(&ctx->error_callback, sizeof(secp256k1_ge) * n_gens);
    for (i = 0; i < n_gens; i++) {
       secp256k1_generator tmpgen;
       unsigned char commit[32] = { 0 };
       commit[0] = i;
       commit[1] = i >> 8;
       commit[2] = i >> 16;
       commit[3] = i >> 24;

       commit[31] = 'G';
       commit[30] = 'H';
       CHECK(secp256k1_generator_generate(ctx, &tmpgen, commit));
       secp256k1_generator_load(&gens[i], &tmpgen);
    }

    test_bulletproof_api();

    /* sanity checks */
    CHECK(secp256k1_bulletproof_innerproduct_proof_length(0) == 32);  /* encoding of 1 */
    CHECK(secp256k1_bulletproof_innerproduct_proof_length(1) == 96);  /* encoding a*b, a, b */
    CHECK(secp256k1_bulletproof_innerproduct_proof_length(2) == 160); /* dot prod, a, b, L, R, parity of L, R */
    CHECK(secp256k1_bulletproof_innerproduct_proof_length(4) == 225); /* dot prod, a, b, a, b, L, R, parity of L, R */
    CHECK(secp256k1_bulletproof_innerproduct_proof_length(8) == 289); /* dot prod, a, b, a, b, L, R, L, R, parity of L, R */

    test_bulletproof_inner_product(0, gens, n_gens);
    test_bulletproof_inner_product(1, gens, n_gens);
    test_bulletproof_inner_product(2, gens, n_gens);
    test_bulletproof_inner_product(4, gens, n_gens);
    test_bulletproof_inner_product(8, gens, n_gens);
    for (i = 0; i < (size_t) count; i++) {
        test_bulletproof_inner_product(32, gens, n_gens);
        test_bulletproof_inner_product(64, gens, n_gens);
    }
    test_bulletproof_inner_product(1024, gens, n_gens);

    test_bulletproof_rangeproof(1, 289, gens, n_gens);
    test_bulletproof_rangeproof(2, 353, gens, n_gens);
    test_bulletproof_rangeproof(16, 546, gens, n_gens);
    test_bulletproof_rangeproof(32, 610, gens, n_gens);
    test_bulletproof_rangeproof(64, 675, gens, n_gens);

    test_bulletproof_rangeproof_aggregate(64, 1, 675, gens, n_gens);
    test_bulletproof_rangeproof_aggregate(8, 2, 546, gens, n_gens);
    test_bulletproof_rangeproof_aggregate(8, 4, 610, gens, n_gens);

    free(gens);
}
#undef MAX_WIDTH

#endif
