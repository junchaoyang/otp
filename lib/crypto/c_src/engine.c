/*
 * %CopyrightBegin%
 *
 * Copyright Ericsson AB 2010-2018. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * %CopyrightEnd%
 */

#include "engine.h"

#ifdef HAS_ENGINE_SUPPORT
struct engine_ctx {
    ENGINE *engine;
    char *id;
};

static ErlNifResourceType* engine_ctx_rtype;

static int get_engine_load_cmd_list(ErlNifEnv* env, const ERL_NIF_TERM term, char **cmds, int i);
static int zero_terminate(ErlNifBinary bin, char **buf);

static void engine_ctx_dtor(ErlNifEnv* env, struct engine_ctx* ctx) {
    PRINTF_ERR0("engine_ctx_dtor");
    if(ctx->id) {
        PRINTF_ERR1("  non empty ctx->id=%s", ctx->id);
        enif_free(ctx->id);
    } else
         PRINTF_ERR0("  empty ctx->id=NULL");
}

int get_engine_and_key_id(ErlNifEnv *env, ERL_NIF_TERM key, char ** id, ENGINE **e)
{
    ERL_NIF_TERM engine_res, key_id_term;
    struct engine_ctx *ctx;
    ErlNifBinary key_id_bin;

    if (!enif_get_map_value(env, key, atom_engine, &engine_res) ||
        !enif_get_resource(env, engine_res, engine_ctx_rtype, (void**)&ctx) ||
        !enif_get_map_value(env, key, atom_key_id, &key_id_term) ||
        !enif_inspect_binary(env, key_id_term, &key_id_bin)) {
        return 0;
    }
    else {
        *e = ctx->engine;
        return zero_terminate(key_id_bin, id);
    }
}

char *get_key_password(ErlNifEnv *env, ERL_NIF_TERM key) {
    ERL_NIF_TERM tmp_term;
    ErlNifBinary pwd_bin;
    char *pwd = NULL;
    if (enif_get_map_value(env, key, atom_password, &tmp_term) &&
        enif_inspect_binary(env, tmp_term, &pwd_bin) &&
        zero_terminate(pwd_bin, &pwd)
        ) return pwd;

    return NULL;
}

static int zero_terminate(ErlNifBinary bin, char **buf) {
    *buf = enif_alloc(bin.size+1);
    if (!*buf)
        return 0;
    memcpy(*buf, bin.data, bin.size);
    *(*buf+bin.size) = 0;
    return 1;
}
#endif /* HAS_ENGINE_SUPPORT */

int init_engine_ctx(ErlNifEnv *env) {
#ifdef HAS_ENGINE_SUPPORT
    engine_ctx_rtype = enif_open_resource_type(env, NULL, "ENGINE_CTX",
                                                   (ErlNifResourceDtor*) engine_ctx_dtor,
                                                   ERL_NIF_RT_CREATE|ERL_NIF_RT_TAKEOVER,
                                                   NULL);
    if (engine_ctx_rtype == NULL) {
        PRINTF_ERR0("CRYPTO: Could not open resource type 'ENGINE_CTX'");
        return 0;
    }
#endif

    return 1;
}

ERL_NIF_TERM engine_by_id_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (EngineId) */
#ifdef HAS_ENGINE_SUPPORT
    ERL_NIF_TERM ret;
    ErlNifBinary engine_id_bin;
    char *engine_id;
    ENGINE *engine;
    struct engine_ctx *ctx;

    // Get Engine Id
    if(!enif_inspect_binary(env, argv[0], &engine_id_bin)) {
        PRINTF_ERR0("engine_by_id_nif Leaved: badarg");
        return enif_make_badarg(env);
    } else {
        engine_id = enif_alloc(engine_id_bin.size+1);
        (void) memcpy(engine_id, engine_id_bin.data, engine_id_bin.size);
        engine_id[engine_id_bin.size] = '\0';
    }

    engine = ENGINE_by_id(engine_id);
    if(!engine) {
        enif_free(engine_id);
        PRINTF_ERR0("engine_by_id_nif Leaved: {error, bad_engine_id}");
        return enif_make_tuple2(env, atom_error, atom_bad_engine_id);
    }

    ctx = enif_alloc_resource(engine_ctx_rtype, sizeof(struct engine_ctx));
    ctx->engine = engine;
    ctx->id = engine_id;

    ret = enif_make_resource(env, ctx);
    enif_release_resource(ctx);

    return enif_make_tuple2(env, atom_ok, ret);
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_init_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    ERL_NIF_TERM ret = atom_ok;
    struct engine_ctx *ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_init_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }
    if (!ENGINE_init(ctx->engine)) {
        //ERR_print_errors_fp(stderr);
        PRINTF_ERR0("engine_init_nif Leaved: {error, engine_init_failed}");
        return enif_make_tuple2(env, atom_error, atom_engine_init_failed);
    }

    return ret;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_free_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    struct engine_ctx *ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_free_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }

    ENGINE_free(ctx->engine);
    return atom_ok;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_finish_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    struct engine_ctx *ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_finish_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }

    ENGINE_finish(ctx->engine);
    return atom_ok;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_load_dynamic_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* () */
#ifdef HAS_ENGINE_SUPPORT
    ENGINE_load_dynamic();
    return atom_ok;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_ctrl_cmd_strings_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine, Commands) */
#ifdef HAS_ENGINE_SUPPORT
    ERL_NIF_TERM ret = atom_ok;
    unsigned int cmds_len = 0;
    char **cmds = NULL;
    struct engine_ctx *ctx;
    int i, optional = 0;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_ctrl_cmd_strings_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }

    PRINTF_ERR1("Engine Id:  %s\r\n", ENGINE_get_id(ctx->engine));

    // Get Command List
    if(!enif_get_list_length(env, argv[1], &cmds_len)) {
        PRINTF_ERR0("engine_ctrl_cmd_strings_nif Leaved: Bad Command List");
        return enif_make_badarg(env);
    } else {
        cmds_len *= 2; // Key-Value list from erlang
        cmds = enif_alloc((cmds_len+1)*sizeof(char*));
        if(get_engine_load_cmd_list(env, argv[1], cmds, 0)) {
            PRINTF_ERR0("engine_ctrl_cmd_strings_nif Leaved: Couldn't read Command List");
            ret = enif_make_badarg(env);
            goto error;
        }
    }

    if(!enif_get_int(env, argv[2], &optional)) {
        PRINTF_ERR0("engine_ctrl_cmd_strings_nif Leaved: Parameter optional not an integer");
        return enif_make_badarg(env);
    }

    for(i = 0; i < cmds_len; i+=2) {
        PRINTF_ERR2("Cmd:  %s:%s\r\n",
                   cmds[i] ? cmds[i] : "(NULL)",
                   cmds[i+1] ? cmds[i+1] : "(NULL)");
        if(!ENGINE_ctrl_cmd_string(ctx->engine, cmds[i], cmds[i+1], optional)) {
            PRINTF_ERR2("Command failed:  %s:%s\r\n",
                        cmds[i] ? cmds[i] : "(NULL)",
                        cmds[i+1] ? cmds[i+1] : "(NULL)");
            //ENGINE_free(ctx->engine);
            ret = enif_make_tuple2(env, atom_error, atom_ctrl_cmd_failed);
            PRINTF_ERR0("engine_ctrl_cmd_strings_nif Leaved: {error, ctrl_cmd_failed}");
            goto error;
        }
    }

 error:
    for(i = 0; cmds != NULL && cmds[i] != NULL; i++)
        enif_free(cmds[i]);
    enif_free(cmds);
    return ret;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_add_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    struct engine_ctx *ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_add_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }

    if (!ENGINE_add(ctx->engine)) {
        PRINTF_ERR0("engine_add_nif Leaved: {error, add_engine_failed}");
        return enif_make_tuple2(env, atom_error, atom_add_engine_failed);
    }
    return atom_ok;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_remove_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    struct engine_ctx *ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_remove_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }

    if (!ENGINE_remove(ctx->engine)) {
        PRINTF_ERR0("engine_remove_nif Leaved: {error, remove_engine_failed}");
        return enif_make_tuple2(env, atom_error, atom_remove_engine_failed);
    }
    return atom_ok;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_register_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine, EngineMethod) */
#ifdef HAS_ENGINE_SUPPORT
    struct engine_ctx *ctx;
    unsigned int method;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_register_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }
    // Get Method
    if (!enif_get_uint(env, argv[1], &method)) {
        PRINTF_ERR0("engine_register_nif Leaved: Parameter Method not an uint");
        return enif_make_badarg(env);
    }

    switch(method)
    {
#ifdef ENGINE_METHOD_RSA
    case ENGINE_METHOD_RSA:
        if (!ENGINE_register_RSA(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_DSA
    case ENGINE_METHOD_DSA:
        if (!ENGINE_register_DSA(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_DH
    case ENGINE_METHOD_DH:
        if (!ENGINE_register_DH(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_RAND
    case ENGINE_METHOD_RAND:
        if (!ENGINE_register_RAND(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_ECDH
    case ENGINE_METHOD_ECDH:
        if (!ENGINE_register_ECDH(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_ECDSA
    case ENGINE_METHOD_ECDSA:
        if (!ENGINE_register_ECDSA(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_STORE
    case ENGINE_METHOD_STORE:
        if (!ENGINE_register_STORE(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_CIPHERS
    case ENGINE_METHOD_CIPHERS:
        if (!ENGINE_register_ciphers(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_DIGESTS
    case ENGINE_METHOD_DIGESTS:
        if (!ENGINE_register_digests(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_PKEY_METHS
    case ENGINE_METHOD_PKEY_METHS:
        if (!ENGINE_register_pkey_meths(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_PKEY_ASN1_METHS
    case ENGINE_METHOD_PKEY_ASN1_METHS:
        if (!ENGINE_register_pkey_asn1_meths(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
#ifdef ENGINE_METHOD_EC
    case ENGINE_METHOD_EC:
        if (!ENGINE_register_EC(ctx->engine))
            return enif_make_tuple2(env, atom_error, atom_register_engine_failed);
        break;
#endif
    default:
        return  enif_make_tuple2(env, atom_error, atom_engine_method_not_supported);
        break;
    }
    return atom_ok;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_unregister_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine, EngineMethod) */
#ifdef HAS_ENGINE_SUPPORT
    struct engine_ctx *ctx;
    unsigned int method;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_unregister_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }
    // Get Method
    if (!enif_get_uint(env, argv[1], &method)) {
        PRINTF_ERR0("engine_unregister_nif Leaved: Parameter Method not an uint");
        return enif_make_badarg(env);
    }

    switch(method)
    {
#ifdef ENGINE_METHOD_RSA
    case ENGINE_METHOD_RSA:
        ENGINE_unregister_RSA(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_DSA
    case ENGINE_METHOD_DSA:
        ENGINE_unregister_DSA(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_DH
    case ENGINE_METHOD_DH:
        ENGINE_unregister_DH(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_RAND
    case ENGINE_METHOD_RAND:
        ENGINE_unregister_RAND(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_ECDH
    case ENGINE_METHOD_ECDH:
        ENGINE_unregister_ECDH(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_ECDSA
    case ENGINE_METHOD_ECDSA:
        ENGINE_unregister_ECDSA(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_STORE
    case ENGINE_METHOD_STORE:
        ENGINE_unregister_STORE(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_CIPHERS
    case ENGINE_METHOD_CIPHERS:
        ENGINE_unregister_ciphers(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_DIGESTS
    case ENGINE_METHOD_DIGESTS:
        ENGINE_unregister_digests(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_PKEY_METHS
    case ENGINE_METHOD_PKEY_METHS:
        ENGINE_unregister_pkey_meths(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_PKEY_ASN1_METHS
    case ENGINE_METHOD_PKEY_ASN1_METHS:
        ENGINE_unregister_pkey_asn1_meths(ctx->engine);
        break;
#endif
#ifdef ENGINE_METHOD_EC
    case ENGINE_METHOD_EC:
        ENGINE_unregister_EC(ctx->engine);
        break;
#endif
    default:
        break;
    }
    return atom_ok;
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_get_first_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    ERL_NIF_TERM ret;
    ENGINE *engine;
    ErlNifBinary engine_bin;
    struct engine_ctx *ctx;

    engine = ENGINE_get_first();
    if(!engine) {
        enif_alloc_binary(0, &engine_bin);
        engine_bin.size = 0;
        return enif_make_tuple2(env, atom_ok, enif_make_binary(env, &engine_bin));
    }

    ctx = enif_alloc_resource(engine_ctx_rtype, sizeof(struct engine_ctx));
    ctx->engine = engine;
    ctx->id = NULL;

    ret = enif_make_resource(env, ctx);
    enif_release_resource(ctx);

    return enif_make_tuple2(env, atom_ok, ret);
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_get_next_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    ERL_NIF_TERM ret;
    ENGINE *engine;
    ErlNifBinary engine_bin;
    struct engine_ctx *ctx, *next_ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_get_next_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }
    engine = ENGINE_get_next(ctx->engine);
    if (!engine) {
        enif_alloc_binary(0, &engine_bin);
        engine_bin.size = 0;
        return enif_make_tuple2(env, atom_ok, enif_make_binary(env, &engine_bin));
    }

    next_ctx = enif_alloc_resource(engine_ctx_rtype, sizeof(struct engine_ctx));
    next_ctx->engine = engine;
    next_ctx->id = NULL;

    ret = enif_make_resource(env, next_ctx);
    enif_release_resource(next_ctx);

    return enif_make_tuple2(env, atom_ok, ret);
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_get_id_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    ErlNifBinary engine_id_bin;
    const char *engine_id;
    int size;
    struct engine_ctx *ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_get_id_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }

    engine_id = ENGINE_get_id(ctx->engine);
    if (!engine_id) {
        enif_alloc_binary(0, &engine_id_bin);
        engine_id_bin.size = 0;
        return enif_make_binary(env, &engine_id_bin);
    }

    size = strlen(engine_id);
    enif_alloc_binary(size, &engine_id_bin);
    engine_id_bin.size = size;
    memcpy(engine_id_bin.data, engine_id, size);

    return enif_make_binary(env, &engine_id_bin);
#else
    return atom_notsup;
#endif
}

ERL_NIF_TERM engine_get_name_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Engine) */
#ifdef HAS_ENGINE_SUPPORT
    ErlNifBinary engine_name_bin;
    const char *engine_name;
    int size;
    struct engine_ctx *ctx;

    // Get Engine
    if (!enif_get_resource(env, argv[0], engine_ctx_rtype, (void**)&ctx)) {
        PRINTF_ERR0("engine_get_id_nif Leaved: Parameter not an engine resource object");
        return enif_make_badarg(env);
    }

    engine_name = ENGINE_get_name(ctx->engine);
    if (!engine_name) {
        enif_alloc_binary(0, &engine_name_bin);
        engine_name_bin.size = 0;
        return enif_make_binary(env, &engine_name_bin);
    }

    size = strlen(engine_name);
    enif_alloc_binary(size, &engine_name_bin);
    engine_name_bin.size = size;
    memcpy(engine_name_bin.data, engine_name, size);

    return enif_make_binary(env, &engine_name_bin);
#else
    return atom_notsup;
#endif
}

#ifdef HAS_ENGINE_SUPPORT
static int get_engine_load_cmd_list(ErlNifEnv* env, const ERL_NIF_TERM term, char **cmds, int i)
{
    ERL_NIF_TERM head, tail;
    const ERL_NIF_TERM *tmp_tuple;
    ErlNifBinary tmpbin;
    int arity;
    char* tmpstr;

    if(!enif_is_empty_list(env, term)) {
        if(!enif_get_list_cell(env, term, &head, &tail)) {
            cmds[i] = NULL;
            return -1;
        } else {
            if(!enif_get_tuple(env, head, &arity, &tmp_tuple)  || arity != 2) {
                cmds[i] = NULL;
                return -1;
            } else {
                if(!enif_inspect_binary(env, tmp_tuple[0], &tmpbin)) {
                    cmds[i] = NULL;
                    return -1;
                } else {
                    tmpstr = enif_alloc(tmpbin.size+1);
                    (void) memcpy(tmpstr, tmpbin.data, tmpbin.size);
                    tmpstr[tmpbin.size] = '\0';
                    cmds[i++] = tmpstr;
                }
                if(!enif_inspect_binary(env, tmp_tuple[1], &tmpbin)) {
                    cmds[i] = NULL;
                    return -1;
                } else {
                    if(tmpbin.size == 0)
                        cmds[i++] = NULL;
                    else {
                        tmpstr = enif_alloc(tmpbin.size+1);
                        (void) memcpy(tmpstr, tmpbin.data, tmpbin.size);
                        tmpstr[tmpbin.size] = '\0';
                        cmds[i++] = tmpstr;
                    }
                }
                return get_engine_load_cmd_list(env, tail, cmds, i);
            }
        }
    } else {
        cmds[i] = NULL;
        return 0;
    }
}
#endif /* HAS_ENGINE_SUPPORT */

ERL_NIF_TERM engine_get_all_methods_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* () */
#ifdef HAS_ENGINE_SUPPORT
    ERL_NIF_TERM method_array[12];
    int i = 0;

#ifdef ENGINE_METHOD_RSA
    method_array[i++] = atom_engine_method_rsa;
#endif
#ifdef ENGINE_METHOD_DSA
    method_array[i++] = atom_engine_method_dsa;
#endif
#ifdef ENGINE_METHOD_DH
    method_array[i++] = atom_engine_method_dh;
#endif
#ifdef ENGINE_METHOD_RAND
    method_array[i++] = atom_engine_method_rand;
#endif
#ifdef ENGINE_METHOD_ECDH
    method_array[i++] = atom_engine_method_ecdh;
#endif
#ifdef ENGINE_METHOD_ECDSA
    method_array[i++] = atom_engine_method_ecdsa;
#endif
#ifdef ENGINE_METHOD_STORE
    method_array[i++] = atom_engine_method_store;
#endif
#ifdef ENGINE_METHOD_CIPHERS
    method_array[i++] = atom_engine_method_ciphers;
#endif
#ifdef ENGINE_METHOD_DIGESTS
    method_array[i++] = atom_engine_method_digests;
#endif
#ifdef ENGINE_METHOD_PKEY_METHS
    method_array[i++] = atom_engine_method_pkey_meths;
#endif
#ifdef ENGINE_METHOD_PKEY_ASN1_METHS
    method_array[i++] = atom_engine_method_pkey_asn1_meths;
#endif
#ifdef ENGINE_METHOD_EC
    method_array[i++] = atom_engine_method_ec;
#endif

    return enif_make_list_from_array(env, method_array, i);
#else
    return atom_notsup;
#endif
}
