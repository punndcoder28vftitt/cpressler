/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <assert.h>

#include <httpd.h>
#include <http_core.h>
#include <http_config.h>
#include <http_log.h>

#include <apr_strings.h>

#include "h2_config.h"
#include "h2_private.h"

#define DEF_VAL     (-1)

#define H2_CONFIG_GET(a, b, n) \
    (((a)->n == DEF_VAL)? (b) : (a))->n

static h2_config defconf = {
    "default",
    0,
    100,
    16 * 1024,
    64 * 1024,
    10,
    256
};

static void *h2_config_create(apr_pool_t *pool,
                              const char *prefix, const char *x)
{
    h2_config *conf = (h2_config *)apr_pcalloc(pool, sizeof(h2_config));
    
    const char *s = x? x : "unknown";
    char *name = (char *)apr_pcalloc(pool, strlen(prefix) + strlen(s) + 20);
    strcpy(name, prefix);
    strcat(name, "[");
    strcat(name, s);
    strcat(name, "]");
    
    conf->name           = name;
    conf->h2_enabled     = DEF_VAL;
    conf->h2_max_streams = DEF_VAL;
    conf->h2_max_hl_size = DEF_VAL;
    conf->h2_window_size = DEF_VAL;
    conf->h2_min_workers = DEF_VAL;
    conf->h2_max_workers = DEF_VAL;
    return conf;
}

void *h2_config_create_svr(apr_pool_t *pool, server_rec *s)
{
    return h2_config_create(pool, "srv", s->defn_name);
}

void *h2_config_create_dir(apr_pool_t *pool, char *x)
{
    return h2_config_create(pool, "dir", x);
}

void *h2_config_merge(apr_pool_t *pool, void *basev, void *addv)
{
    h2_config *base = (h2_config *)basev;
    h2_config *add = (h2_config *)addv;
    h2_config *n = (h2_config *)apr_pcalloc(pool, sizeof(h2_config));

    char *name = (char *)apr_pcalloc(pool,
        20 + strlen(add->name) + strlen(base->name));
    strcpy(name, "merged[");
    strcat(name, add->name);
    strcat(name, ", ");
    strcat(name, base->name);
    strcat(name, "]");
    n->name = name;

    n->h2_enabled     = H2_CONFIG_GET(add, base, h2_enabled);
    n->h2_max_streams = H2_CONFIG_GET(add, base, h2_max_streams);
    n->h2_max_hl_size = H2_CONFIG_GET(add, base, h2_max_hl_size);
    n->h2_window_size = H2_CONFIG_GET(add, base, h2_window_size);
    n->h2_min_workers = H2_CONFIG_GET(add, base, h2_min_workers);
    n->h2_max_workers = H2_CONFIG_GET(add, base, h2_max_workers);

    return n;
}

int h2_config_geti(h2_config *conf, h2_config_var_t var)
{
    switch(var) {
        case H2_CONF_ENABLED:
            return H2_CONFIG_GET(conf, &defconf, h2_enabled);
        case H2_CONF_MAX_STREAMS:
            return H2_CONFIG_GET(conf, &defconf, h2_max_streams);
        case H2_CONF_MAX_HL_SIZE:
            return H2_CONFIG_GET(conf, &defconf, h2_max_hl_size);
        case H2_CONF_WIN_SIZE:
            return H2_CONFIG_GET(conf, &defconf, h2_window_size);
        case H2_CONF_MIN_WORKERS:
            return H2_CONFIG_GET(conf, &defconf, h2_min_workers);
        case H2_CONF_MAX_WORKERS:
            return H2_CONFIG_GET(conf, &defconf, h2_max_workers);
        default:
            return DEF_VAL;
    }
}

static const char *h2_conf_set_engine(cmd_parms *parms,
                                      void *arg, const char *value)
{
    h2_config *cfg = h2_config_sget(parms->server);
    cfg->h2_enabled = !strcasecmp(value, "On");
    return NULL;
}

static const char *h2_conf_set_max_streams(cmd_parms *parms,
                                           void *arg, const char *value)
{
    h2_config *cfg = h2_config_sget(parms->server);
    cfg->h2_max_streams = (int)apr_atoi64(value);
    return NULL;
}

static const char *h2_conf_set_window_size(cmd_parms *parms,
                                           void *arg, const char *value)
{
    h2_config *cfg = h2_config_sget(parms->server);
    cfg->h2_window_size = (int)apr_atoi64(value);
    return NULL;
}

static const char *h2_conf_set_max_hl_size(cmd_parms *parms,
                                           void *arg, const char *value)
{
    h2_config *cfg = h2_config_sget(parms->server);
    cfg->h2_max_hl_size = (int)apr_atoi64(value);
    return NULL;
}

static const char *h2_conf_set_min_workers(cmd_parms *parms,
                                           void *arg, const char *value)
{
    h2_config *cfg = h2_config_sget(parms->server);
    cfg->h2_min_workers = (int)apr_atoi64(value);
    return NULL;
}

static const char *h2_conf_set_max_workers(cmd_parms *parms,
                                           void *arg, const char *value)
{
    h2_config *cfg = h2_config_sget(parms->server);
    cfg->h2_max_workers = (int)apr_atoi64(value);
    return NULL;
}

const command_rec h2_cmds[] = {
    AP_INIT_TAKE1("H2Engine", h2_conf_set_engine, NULL,
                  RSRC_CONF, "on to enable HTTP/2 protocol handling"),
    AP_INIT_TAKE1("H2MaxSessionStreams", h2_conf_set_max_streams, NULL,
                  RSRC_CONF, "maximum number of open streams per session"),
    AP_INIT_TAKE1("H2InitialWindowSize", h2_conf_set_window_size, NULL,
                  RSRC_CONF, "initial window size on client DATA"),
    AP_INIT_TAKE1("H2MaxHeaderListSize", h2_conf_set_max_hl_size, NULL, 
                  RSRC_CONF, "maximum acceptable size of request headers"),
    AP_INIT_TAKE1("H2MinWorkers", h2_conf_set_min_workers, NULL,
                  RSRC_CONF, "minimum number of worker threads per child"),
    AP_INIT_TAKE1("H2MaxWorkers", h2_conf_set_max_workers, NULL,
                  RSRC_CONF, "maximum number of worker threads per child"),
    {NULL}
};


h2_config *h2_config_sget(server_rec *s)
{
    h2_config *cfg = (h2_config *)ap_get_module_config(s->module_config, &h2_module);
    assert(cfg);
    return cfg;
}

h2_config *h2_config_get(conn_rec *c)
{
    return h2_config_sget(c->base_server);
}

