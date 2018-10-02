/**********************************************************************
 * Copyright (c) 2018 Andrew Poelstra                                 *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _SECP256K1_MODULE_MUSIG_MAIN_
#define _SECP256K1_MODULE_MUSIG_MAIN_

#include "include/secp256k1.h"
#include "include/secp256k1_musig.h"
#include "hash.h"

/* TODO: remove */
static int taproot_hash_default(unsigned char *tweak32, const secp256k1_pubkey *pk, const unsigned char *commit, void *data) {
    unsigned char buf[33];
    size_t buflen = sizeof(buf);
    secp256k1_ge pkg;
    secp256k1_sha256 sha;

    (void) data;

    secp256k1_pubkey_load(NULL, &pkg, pk);
    secp256k1_eckey_pubkey_serialize(&pkg, buf, &buflen, 1);

    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, buf, 33);
    secp256k1_sha256_write(&sha, commit, 32);
    secp256k1_sha256_finalize(&sha, tweak32);

    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, tweak32, 32);
    secp256k1_sha256_finalize(&sha, tweak32);

    return 1;
}

/* TODO: remove */
const secp256k1_taproot_hash_function secp256k1_taproot_hash_default = taproot_hash_default;

/* Partial signature data structure:
 * 32 bytes partial s
 * 1 byte indicating whether the public nonce should be flipped
 *
 * Aux data structure:
 * 32 bytes message hash
 * 32 bytes R.x
 */

static void secp256k1_musig_coefficient(secp256k1_scalar *r, const unsigned char *ell, size_t idx) {
    secp256k1_sha256 sha;
    unsigned char buf[32];
    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, ell, 32);
    while (idx > 0) {
        unsigned char c = idx;
        secp256k1_sha256_write(&sha, &c, 1);
        idx /= 0x100;
    }
    secp256k1_sha256_finalize(&sha, buf);

    secp256k1_scalar_set_b32(r, buf, NULL);
}

static int secp256k1_musig_compute_ell(const secp256k1_context *ctx, unsigned char *ell, const secp256k1_pubkey *pk, size_t np) {
    secp256k1_sha256 sha;
    size_t i;

    secp256k1_sha256_initialize(&sha);
    for (i = 0; i < np; i++) {
        unsigned char ser[33];
        size_t serlen = sizeof(ser);
        if (!secp256k1_ec_pubkey_serialize(ctx, ser, &serlen, &pk[i], SECP256K1_EC_COMPRESSED)) {
            return 0;
        }
        secp256k1_sha256_write(&sha, ser, serlen);
    }
    secp256k1_sha256_finalize(&sha, ell);
    return 1;
}

int secp256k1_musig_pubkey_combine(const secp256k1_context* ctx, secp256k1_pubkey *tweaked_pk, secp256k1_pubkey *combined_pk, const secp256k1_pubkey *pk, size_t np) {
    size_t i;
    unsigned char ell[32];
    secp256k1_gej musigj;
    secp256k1_ge musigp;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(combined_pk != NULL);
    ARG_CHECK(pk != NULL);

    if (!secp256k1_musig_compute_ell(ctx, ell, pk, np)) {
        return 0;
    }

    secp256k1_gej_set_infinity(&musigj);
    for (i = 0; i < np; i++) {
        secp256k1_gej termj;
        secp256k1_gej pkj;
        secp256k1_ge pkp;
        secp256k1_scalar mc;

        if (!secp256k1_pubkey_load(ctx, &pkp, &pk[i])) {
            return 0;
        }
        secp256k1_gej_set_ge(&pkj, &pkp);
        secp256k1_musig_coefficient(&mc, ell, i);
        secp256k1_ecmult(&ctx->ecmult_ctx, &termj, &pkj, &mc, NULL);

        secp256k1_gej_add_var(&musigj, &musigj, &termj, NULL);

        if (tweaked_pk != NULL) {
            secp256k1_ge_set_gej(&pkp, &termj);
            secp256k1_pubkey_save(&tweaked_pk[i], &pkp);
        }
    }
    if (secp256k1_gej_is_infinity(&musigj)) {
        return 0;
    }

    secp256k1_ge_set_gej(&musigp, &musigj);
    secp256k1_pubkey_save(combined_pk, &musigp);
    return 1;
}

int secp256k1_musig_init(const secp256k1_context* ctx, secp256k1_scratch *scratch, secp256k1_musig_config *musig_config, const secp256k1_pubkey *pks, const size_t k, const size_t n, const unsigned char *commitment, secp256k1_taproot_hash_function hashfp, void *hdata) {
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(scratch != NULL);
    ARG_CHECK(musig_config != NULL);
    ARG_CHECK(pks != NULL);

    musig_config->scratch = scratch;

    if (k == 0 || k > n) {
        return 0;
    }
    musig_config->k = k;
    musig_config->n = n;

    if (secp256k1_scratch_allocate_frame(scratch, n * sizeof(secp256k1_pubkey), 1) == 0) {
        return 0;
    }
    musig_config->musig_pks = (secp256k1_pubkey *)secp256k1_scratch_alloc(scratch, n * sizeof(secp256k1_pubkey));
    if (!secp256k1_musig_pubkey_combine(ctx, musig_config->musig_pks, &musig_config->combined_pk, pks, musig_config->n)) {
        secp256k1_scratch_deallocate_frame(scratch);
        return 0;
    }

    if (hashfp == NULL) {
        hashfp = secp256k1_taproot_hash_default;
    }
    if (commitment != NULL) {
        musig_config->taproot_tweaked = 1;
        if (!hashfp(musig_config->taproot_tweak, &musig_config->combined_pk, commitment, hdata)) {
            secp256k1_scratch_deallocate_frame(scratch);
            return 0;
        }
        if (!secp256k1_ec_pubkey_tweak_add(ctx, &musig_config->combined_pk, musig_config->taproot_tweak)) {
            secp256k1_scratch_deallocate_frame(scratch);
            return 0;
        }
    } else {
        musig_config->taproot_tweaked = 0;
    }

    return 1;
}

int secp256k1_musig_config_destroy(const secp256k1_context* ctx, secp256k1_musig_config *musig_config) {
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(musig_config != NULL);

    secp256k1_scratch_deallocate_frame(musig_config->scratch);
    return 1;
}

int secp256k1_musig_pubkey(const secp256k1_context* ctx, secp256k1_pubkey *combined_pk, const secp256k1_musig_config *musig_config) {
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(combined_pk != NULL);
    ARG_CHECK(musig_config != NULL);

    memcpy(combined_pk, &musig_config->combined_pk, sizeof(secp256k1_pubkey));
    return 1;
}

int secp256k1_musig_tweaked_pubkeys(const secp256k1_context* ctx, secp256k1_pubkey *musig_pks, const secp256k1_musig_config *musig_config) {
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(musig_pks != NULL);
    ARG_CHECK(musig_config != NULL);

    memcpy(musig_pks, musig_config->musig_pks, musig_config->n * sizeof(secp256k1_pubkey));
    return 1;
}

int secp256k1_musig_tweak_secret_key(const secp256k1_context* ctx, secp256k1_musig_secret_key *out, const unsigned char *seckey, const secp256k1_pubkey *pk, size_t np, size_t my_index) {
    int overflow;
    unsigned char ell[32];
    secp256k1_scalar x, y;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(out != NULL);
    ARG_CHECK(seckey != NULL);
    ARG_CHECK(pk != NULL);

    secp256k1_scalar_set_b32(&x, seckey, &overflow);
    if (overflow) {
        return 0;
    }

    if (!secp256k1_musig_compute_ell(ctx, ell, pk, np)) {
        return 0;
    }
    secp256k1_musig_coefficient(&y, ell, my_index);

    secp256k1_scalar_mul(&x, &x, &y);
    secp256k1_scalar_get_b32(out->data, &x);

    return 1;
}

int secp256k1_musig_signer_data_initialize(const secp256k1_context* ctx, secp256k1_musig_signer_data *data, const secp256k1_pubkey *pubkey, const unsigned char *noncommit) {
    (void) ctx;
    ARG_CHECK(data != NULL);
    ARG_CHECK(pubkey != NULL);
    memset(data, 0, sizeof(*data));
    memcpy(&data->pubkey, pubkey, sizeof(*pubkey));
    if (noncommit != NULL) {
        memcpy(data->noncommit, noncommit, 32);
    }
    return 1;
}

int secp256k1_musig_multisig_generate_nonce(const secp256k1_context* ctx, unsigned char *secnon, secp256k1_pubkey *pubnon, unsigned char *noncommit, const secp256k1_musig_secret_key *seckey, const unsigned char *msg32, const unsigned char *rngseed) {
    unsigned char commit[33];
    size_t commit_size = sizeof(commit);
    secp256k1_sha256 sha;
    secp256k1_scalar secs;
    secp256k1_gej rj;
    secp256k1_ge rp;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    ARG_CHECK(secnon != NULL);
    ARG_CHECK(pubnon != NULL);
    ARG_CHECK(seckey != NULL);
    ARG_CHECK(msg32 != NULL);
    ARG_CHECK(rngseed != NULL);

    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, seckey->data, 32);
    secp256k1_sha256_write(&sha, msg32, 32);
    secp256k1_sha256_write(&sha, rngseed, 32);
    secp256k1_sha256_finalize(&sha, secnon);

    secp256k1_scalar_set_b32(&secs, secnon, NULL);
    secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &rj, &secs);
    secp256k1_ge_set_gej(&rp, &rj);
    secp256k1_pubkey_save(pubnon, &rp);

    if (noncommit != NULL) {
        secp256k1_sha256_initialize(&sha);
        secp256k1_ec_pubkey_serialize(ctx, commit, &commit_size, pubnon, SECP256K1_EC_COMPRESSED);
        secp256k1_sha256_write(&sha, commit, commit_size);
        secp256k1_sha256_finalize(&sha, noncommit);
    }
    return 1;
}

int secp256k1_musig_set_nonce(const secp256k1_context* ctx, secp256k1_musig_signer_data *data, const secp256k1_pubkey *pubnon) {
    unsigned char commit[33];
    size_t commit_size = sizeof(commit);
    secp256k1_sha256 sha;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(data != NULL);
    ARG_CHECK(pubnon != NULL);

    secp256k1_sha256_initialize(&sha);
    secp256k1_ec_pubkey_serialize(ctx, commit, &commit_size, pubnon, SECP256K1_EC_COMPRESSED);
    secp256k1_sha256_write(&sha, commit, commit_size);
    secp256k1_sha256_finalize(&sha, commit);

    if (memcmp(commit, data->noncommit, 32) != 0) {
        return 0;
    }
    memcpy(&data->pubnon, pubnon, sizeof(*pubnon));
    data->present = 1;
    return 1;
}

/* `data` is just used as a binary array indicating which signers are present, i.e.
 * which ones to exclude from the interpolation. */
/* TODO: remove */
static void secp256k1_musig_lagrange_coefficient(secp256k1_scalar *r, const secp256k1_musig_signer_data *data, size_t n_indices, size_t coeff_index, int invert) {
    size_t i;
    int kofn;
    secp256k1_scalar num;
    secp256k1_scalar den;
    secp256k1_scalar indexs;

    /* Special-case the n-of-n case where all our "Lagrange coefficients" should be 1,
     * since in that case we do a simple sum rather than polynomial interpolation */
    kofn = 0;
    for (i = 0; i < n_indices; i++) {
        if (data[i].present == 0) {
            kofn = 1;
            break;
        }
    }
    if (!kofn) {
        secp256k1_scalar_set_int(r, 1);
        return;
    }

    secp256k1_scalar_set_int(&num, 1);
    secp256k1_scalar_set_int(&den, 1);
    secp256k1_scalar_set_int(&indexs, (int) coeff_index + 1);
    for (i = 0; i < n_indices; i++) {
        secp256k1_scalar mul;
        if ((data[i].present == 0) || i == coeff_index) {
            continue;
        }

        secp256k1_scalar_set_int(&mul, (int) i + 1);
        secp256k1_scalar_negate(&mul, &mul);
        secp256k1_scalar_mul(&num, &num, &mul);

        secp256k1_scalar_add(&mul, &mul, &indexs);
        secp256k1_scalar_mul(&den, &den, &mul);
    }

    if (invert) {
        secp256k1_scalar_inverse_var(&num, &num);
    } else {
        secp256k1_scalar_inverse_var(&den, &den);
    }
    secp256k1_scalar_mul(r, &num, &den);
}


typedef struct {
    size_t index;
    size_t n_signers;
    const secp256k1_musig_signer_data *data;
} secp256k1_musig_nonce_ecmult_context;

static int secp256k1_musig_nonce_ecmult_callback(secp256k1_scalar *sc, secp256k1_ge *pt, size_t idx, void *data) {
    secp256k1_musig_nonce_ecmult_context *ctx = (secp256k1_musig_nonce_ecmult_context *) data;

    (void) idx;
    while (!ctx->data[ctx->index].present) {
        ctx->index++;
    }
    secp256k1_musig_lagrange_coefficient(sc, ctx->data, ctx->n_signers, ctx->index, 0);
    if (!secp256k1_pubkey_load(NULL, pt, &ctx->data[ctx->index].pubnon)) {
        return 0;
    }
    ctx->index++;
    return 1;
}

int secp256k1_musig_partial_sign(const secp256k1_context* ctx, secp256k1_scratch_space *scratch, secp256k1_musig_partial_signature *partial_sig, secp256k1_musig_validation_aux *aux, unsigned char *secnon, const secp256k1_musig_config *musig_config, const secp256k1_musig_secret_key *seckey, const unsigned char *msg32, const secp256k1_musig_signer_data *data, size_t my_index, const unsigned char *sec_adaptor) {
    secp256k1_musig_nonce_ecmult_context ecmult_data;
    unsigned char buf[33];
    size_t bufsize = 33;
    secp256k1_gej total_rj;
    secp256k1_ge total_r;
    secp256k1_sha256 sha;
    size_t i;
    size_t n_present;
    int overflow;
    secp256k1_scalar sk;
    secp256k1_scalar e, k;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(scratch != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(aux != NULL);
    ARG_CHECK(musig_config != NULL);
    ARG_CHECK(seckey != NULL);
    ARG_CHECK(msg32 != NULL);
    ARG_CHECK(secnon != NULL);
    ARG_CHECK(data != NULL);

    /* Should this be an ARG_CHECK ? */
    if (!data[my_index].present) {
        return 0;
    }

    secp256k1_scalar_set_b32(&sk, seckey->data, &overflow);
    if (overflow) {
        return 0;
    }

    secp256k1_scalar_set_b32(&k, secnon, &overflow);
    if (overflow || secp256k1_scalar_is_zero(&k)) {
        return 0;
    }

    /* compute aggregate R, saving partial-R in the partial_signature structure */
    n_present = 0;
    for (i = 0; i < musig_config->n; i++) {
        if (data[i].present != 0) {
            n_present++;
        }
    }
    if (n_present < musig_config->k) {
        return 0;
    }
    ecmult_data.index = 0;
    ecmult_data.n_signers = musig_config->n;
    ecmult_data.data = data;
    if (!secp256k1_ecmult_multi_var(&ctx->ecmult_ctx, scratch, &total_rj, NULL, secp256k1_musig_nonce_ecmult_callback, (void *) &ecmult_data, n_present)) {
        return 0;
    }
    if (secp256k1_gej_is_infinity(&total_rj)) {
        return 0;
    }
    if (!secp256k1_gej_has_quad_y_var(&total_rj)) {
        secp256k1_gej_neg(&total_rj, &total_rj);
        secp256k1_scalar_negate(&k, &k);
        partial_sig->data[32] = 1;
    } else {
        partial_sig->data[32] = 0;
    }
    secp256k1_ge_set_gej(&total_r, &total_rj);

    /* build message hash */
    secp256k1_sha256_initialize(&sha);
    secp256k1_fe_normalize(&total_r.x);
    secp256k1_fe_get_b32(buf, &total_r.x);
    secp256k1_sha256_write(&sha, buf, 32);
    memcpy(&aux->data[32], buf, 32);
    secp256k1_ec_pubkey_serialize(ctx, buf, &bufsize, &musig_config->combined_pk, SECP256K1_EC_COMPRESSED);
    VERIFY_CHECK(bufsize == 33);
    secp256k1_sha256_write(&sha, buf, bufsize);
    secp256k1_sha256_write(&sha, msg32, 32);
    secp256k1_sha256_finalize(&sha, aux->data);

    secp256k1_scalar_set_b32(&e, aux->data, NULL);

    /* Sign */
    secp256k1_scalar_mul(&e, &e, &sk);
    secp256k1_scalar_add(&e, &e, &k);
    if (sec_adaptor != NULL) {
        secp256k1_scalar offs;
        secp256k1_scalar_set_b32(&offs, sec_adaptor, &overflow);
        if (overflow) {
            return 0;
        }
        secp256k1_scalar_negate(&offs, &offs);
        secp256k1_scalar_add(&e, &e, &offs);
    }
    secp256k1_scalar_get_b32(&partial_sig->data[0], &e);
    secp256k1_scalar_clear(&sk);
    secp256k1_scalar_clear(&k);
    /* Set secnon to zero such that consecutive partial signing attempts fail */
    if (sec_adaptor == NULL) {
        secp256k1_scalar_get_b32(secnon, &k);
    }

    return 1;
}

int secp256k1_musig_partial_sig_combine(const secp256k1_context* ctx, secp256k1_schnorrsig *sig, const secp256k1_musig_config *musig_config, const secp256k1_musig_partial_signature *partial_sig, size_t n_sigs, const secp256k1_musig_signer_data *data, const secp256k1_musig_validation_aux *aux) {
    size_t i, j;
    secp256k1_scalar s;
    secp256k1_ge rp;
    secp256k1_gej rj;
    (void) ctx;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(sig != NULL);
    ARG_CHECK(musig_config != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(data != NULL);
    ARG_CHECK(aux != NULL);
    ARG_CHECK(musig_config->n >= n_sigs);

    secp256k1_scalar_clear(&s);
    secp256k1_gej_set_infinity(&rj);
    j = 0;
    for (i = 0; i < musig_config->n; i++) {
        int overflow;
        secp256k1_scalar term;
        secp256k1_scalar coeff;

        if (!data[i].present) {
            continue;
        }

        secp256k1_scalar_set_b32(&term, partial_sig[j].data, &overflow);
        if (overflow) {
            return 0;
        }
        if (!secp256k1_pubkey_load(ctx, &rp, &data[i].pubnon)) {
            return 0;
        }

        secp256k1_musig_lagrange_coefficient(&coeff, data, musig_config->n, i, 0);
        secp256k1_scalar_mul(&term, &term, &coeff);
        secp256k1_scalar_add(&s, &s, &term);
        j++;
    }
    if (j != n_sigs) {
        return 0;
    }

    /* Add taproot tweak to final signature */
    if (musig_config->taproot_tweaked) {
        secp256k1_scalar tweaks;
        secp256k1_scalar e;

        secp256k1_scalar_set_b32(&tweaks, musig_config->taproot_tweak, NULL);
        secp256k1_scalar_set_b32(&e, aux->data, NULL);
        secp256k1_scalar_mul(&tweaks, &tweaks, &e);
        secp256k1_scalar_add(&s, &s, &tweaks);
    }

    memcpy(&sig->data[0], &aux->data[32], 32);
    secp256k1_scalar_get_b32(&sig->data[32], &s);

    return 1;
}

int secp256k1_musig_adaptor_signature_extract_secret(const secp256k1_context* ctx, unsigned char *sec_adaptor, const secp256k1_musig_config *musig_config, const secp256k1_schnorrsig *full_sig, const secp256k1_musig_partial_signature *partial_sig, const secp256k1_musig_partial_signature *adaptor_sig, const secp256k1_musig_validation_aux *aux) {
    secp256k1_scalar s;
    secp256k1_scalar term;
    int overflow;
    (void) ctx;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(sec_adaptor != NULL);
    ARG_CHECK(musig_config != NULL);
    ARG_CHECK(full_sig != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(adaptor_sig != NULL);
    if (musig_config->taproot_tweaked) {
        ARG_CHECK(aux != NULL);
    }

    secp256k1_scalar_set_b32(&s, &full_sig->data[32], &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_set_b32(&term, partial_sig->data, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_negate(&term, &term);
    secp256k1_scalar_add(&s, &s, &term);
    secp256k1_scalar_set_b32(&term, adaptor_sig->data, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_negate(&term, &term);
    secp256k1_scalar_add(&s, &s, &term);

    if (musig_config->taproot_tweaked) {
        secp256k1_scalar tweaks;
        secp256k1_scalar e;

        secp256k1_scalar_set_b32(&tweaks, musig_config->taproot_tweak, NULL);
        secp256k1_scalar_set_b32(&e, aux->data, NULL);
        secp256k1_scalar_mul(&tweaks, &tweaks, &e);
        secp256k1_scalar_negate(&tweaks, &tweaks);
        secp256k1_scalar_add(&s, &s, &tweaks);
    }

    secp256k1_scalar_get_b32(sec_adaptor, &s);
    return 1;
}

int secp256k1_musig_adaptor_signature_adapt(const secp256k1_context* ctx, secp256k1_musig_partial_signature *partial_sig, const secp256k1_musig_partial_signature *adaptor_sig, const unsigned char *sec_adaptor) {
    secp256k1_scalar s;
    secp256k1_scalar t;
    int overflow;

    (void) ctx;
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(adaptor_sig != NULL);
    ARG_CHECK(sec_adaptor != NULL);

    secp256k1_scalar_set_b32(&s, adaptor_sig->data, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_set_b32(&t, sec_adaptor, &overflow);
    if (overflow) {
        return 0;
    }

    secp256k1_scalar_add(&s, &s, &t);
    secp256k1_scalar_get_b32(partial_sig->data, &s);
    partial_sig->data[32] = adaptor_sig->data[32];

    return 1;
}

int secp256k1_musig_adaptor_signature_apply_secret(const secp256k1_context* ctx, secp256k1_schnorrsig *partial_sig, const secp256k1_schnorrsig *adaptor_sig, const unsigned char *sec_adaptor) {
    secp256k1_scalar s;
    secp256k1_scalar term;
    int overflow;
    (void) ctx;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(adaptor_sig != NULL);
    ARG_CHECK(sec_adaptor != NULL);

    secp256k1_scalar_set_b32(&s, &adaptor_sig->data[32], &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_set_b32(&term, sec_adaptor, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_add(&s, &s, &term);

    memcpy(&partial_sig->data[0], &adaptor_sig->data[0], 32);
    secp256k1_scalar_get_b32(&partial_sig->data[32], &s);
    return 1;
}

int secp256k1_musig_adaptor_signature_extract(const secp256k1_context* ctx, secp256k1_pubkey *pub_adaptor, const secp256k1_musig_partial_signature *partial_sig, const secp256k1_musig_signer_data *data, const secp256k1_musig_validation_aux *aux) {
    secp256k1_scalar s;
    secp256k1_scalar e;
    secp256k1_gej rj;
    secp256k1_ge rp;
    int overflow;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(pub_adaptor != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(data != NULL);
    ARG_CHECK(aux != NULL);

    if (!data->present) {
        return 0;
    }
    secp256k1_scalar_set_b32(&s, partial_sig->data, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_set_b32(&e, aux->data, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_pubkey_load(ctx, &rp, &data->pubkey);

    if (!secp256k1_pubkey_load(ctx, &rp, &data->pubnon)) {
        return 0;
    }

    if (!secp256k1_schnorrsig_real_verify(ctx, &rj, &s, &e, &data->pubkey)) {
        return 0;
    }
    if (!partial_sig->data[32]) {
        secp256k1_ge_neg(&rp, &rp);
    }

    secp256k1_gej_add_ge_var(&rj, &rj, &rp, NULL);
    if (secp256k1_gej_is_infinity(&rj)) {
        return 0;
    }

    secp256k1_ge_set_gej(&rp, &rj);
    secp256k1_pubkey_save(pub_adaptor, &rp);
    return 1;
}

int secp256k1_musig_partial_sig_verify(const secp256k1_context* ctx, const secp256k1_musig_partial_signature *partial_sig, const secp256k1_musig_signer_data *data, const secp256k1_musig_validation_aux *aux) {
    secp256k1_scalar s;
    secp256k1_scalar e;
    secp256k1_gej rj;
    secp256k1_ge rp;
    int overflow;

    printf("0\n");
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(data != NULL);
    ARG_CHECK(aux != NULL);

    printf("A\n");
    if (!data->present) {
        return 0;
    }
    printf("b\n");
    secp256k1_scalar_set_b32(&s, partial_sig->data, &overflow);
    if (overflow) {
        return 0;
    }
    printf("c\n");
    secp256k1_scalar_set_b32(&e, aux->data, &overflow);
    if (overflow) {
        return 0;
    }
    printf("d\n");
    if (!secp256k1_pubkey_load(ctx, &rp, &data->pubnon)) {
        return 0;
    }

    printf("e\n");
    if (!secp256k1_schnorrsig_real_verify(ctx, &rj, &s, &e, &data->pubkey)) {
        return 0;
    }
    printf("f\n");
    if (!partial_sig->data[32]) {
        secp256k1_ge_neg(&rp, &rp);
    }
    printf("g\n");
    secp256k1_gej_add_ge_var(&rj, &rj, &rp, NULL);

    return secp256k1_gej_is_infinity(&rj);
}

#endif
