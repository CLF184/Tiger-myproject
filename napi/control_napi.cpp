#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"

#include "auto_control.h"

namespace {

bool GetBoolArg(napi_env env, napi_value v, bool *out)
{
    if (!out) return false;
    napi_valuetype t;
    if (napi_typeof(env, v, &t) != napi_ok || t != napi_boolean) {
        return false;
    }
    return napi_get_value_bool(env, v, out) == napi_ok;
}

bool GetOptionalNumberProp(napi_env env, napi_value obj, const char *name, bool *present, double *out)
{
    if (present) *present = false;
    if (!out || !name) return false;

    bool has = false;
    if (napi_has_named_property(env, obj, name, &has) != napi_ok || !has) {
        return true;
    }

    napi_value v;
    if (napi_get_named_property(env, obj, name, &v) != napi_ok) {
        return false;
    }

    napi_valuetype t;
    if (napi_typeof(env, v, &t) != napi_ok || t != napi_number) {
        return false;
    }

    if (napi_get_value_double(env, v, out) != napi_ok) {
        return false;
    }

    if (present) *present = true;
    return true;
}

static napi_value setAutoControlEnabled(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    bool enabled = false;
    if (argc >= 1) {
        (void)GetBoolArg(env, args[0], &enabled);
    }

    control::SetAutoControlEnabled(enabled);

    napi_value result;
    NAPI_CALL(env, napi_create_int32(env, 0, &result));
    return result;
}

static napi_value getAutoControlEnabled(napi_env env, napi_callback_info info)
{
    (void)info;
    napi_value result;
    NAPI_CALL(env, napi_get_boolean(env, control::GetAutoControlEnabled(), &result));
    return result;
}

static napi_value setAutoControlThresholds(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    control::AutoControlThresholds t = control::GetThresholds();

    if (argc >= 1) {
        napi_valuetype ot;
        NAPI_CALL(env, napi_typeof(env, args[0], &ot));
        if (ot == napi_object) {
            bool present = false;
            double v = 0;

            if (GetOptionalNumberProp(env, args[0], "soil_on", &present, &v) && present) t.soil_on = static_cast<int>(v);
            if (GetOptionalNumberProp(env, args[0], "soil_off", &present, &v) && present) t.soil_off = static_cast<int>(v);
            if (GetOptionalNumberProp(env, args[0], "light_on", &present, &v) && present) t.light_on = static_cast<int>(v);
            if (GetOptionalNumberProp(env, args[0], "light_off", &present, &v) && present) t.light_off = static_cast<int>(v);
            if (GetOptionalNumberProp(env, args[0], "temp_on", &present, &v) && present) t.temp_on = v;
            if (GetOptionalNumberProp(env, args[0], "temp_off", &present, &v) && present) t.temp_off = v;
            if (GetOptionalNumberProp(env, args[0], "ch2o_on", &present, &v) && present) t.ch2o_on = v;
            if (GetOptionalNumberProp(env, args[0], "ch2o_off", &present, &v) && present) t.ch2o_off = v;
            if (GetOptionalNumberProp(env, args[0], "co2_on", &present, &v) && present) t.co2_on = v;
            if (GetOptionalNumberProp(env, args[0], "co2_off", &present, &v) && present) t.co2_off = v;
            if (GetOptionalNumberProp(env, args[0], "co2_night_on", &present, &v) && present) t.co2_night_on = v;
            if (GetOptionalNumberProp(env, args[0], "co2_night_off", &present, &v) && present) t.co2_night_off = v;
            if (GetOptionalNumberProp(env, args[0], "ph_min", &present, &v) && present) t.ph_min = v;
            if (GetOptionalNumberProp(env, args[0], "ph_max", &present, &v) && present) t.ph_max = v;
            if (GetOptionalNumberProp(env, args[0], "ec_min", &present, &v) && present) t.ec_min = v;
            if (GetOptionalNumberProp(env, args[0], "ec_max", &present, &v) && present) t.ec_max = v;
            if (GetOptionalNumberProp(env, args[0], "n_min", &present, &v) && present) t.n_min = v;
            if (GetOptionalNumberProp(env, args[0], "n_max", &present, &v) && present) t.n_max = v;
            if (GetOptionalNumberProp(env, args[0], "p_min", &present, &v) && present) t.p_min = v;
            if (GetOptionalNumberProp(env, args[0], "p_max", &present, &v) && present) t.p_max = v;
            if (GetOptionalNumberProp(env, args[0], "k_min", &present, &v) && present) t.k_min = v;
            if (GetOptionalNumberProp(env, args[0], "k_max", &present, &v) && present) t.k_max = v;
            if (GetOptionalNumberProp(env, args[0], "fan_speed", &present, &v) && present) t.fan_speed = static_cast<int>(v);
        }
    }

    control::SetThresholds(t);

    napi_value result;
    NAPI_CALL(env, napi_create_int32(env, 0, &result));
    return result;
}

static napi_value getAutoControlThresholds(napi_env env, napi_callback_info info)
{
    (void)info;

    control::AutoControlThresholds t = control::GetThresholds();

    napi_value obj;
    NAPI_CALL(env, napi_create_object(env, &obj));

    napi_value v;
    NAPI_CALL(env, napi_create_int32(env, t.soil_on, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "soil_on", v));
    NAPI_CALL(env, napi_create_int32(env, t.soil_off, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "soil_off", v));

    NAPI_CALL(env, napi_create_int32(env, t.light_on, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "light_on", v));
    NAPI_CALL(env, napi_create_int32(env, t.light_off, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "light_off", v));

    NAPI_CALL(env, napi_create_double(env, t.temp_on, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "temp_on", v));
    NAPI_CALL(env, napi_create_double(env, t.temp_off, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "temp_off", v));

    NAPI_CALL(env, napi_create_double(env, t.ch2o_on, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "ch2o_on", v));
    NAPI_CALL(env, napi_create_double(env, t.ch2o_off, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "ch2o_off", v));

    NAPI_CALL(env, napi_create_double(env, t.co2_on, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "co2_on", v));
    NAPI_CALL(env, napi_create_double(env, t.co2_off, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "co2_off", v));

    NAPI_CALL(env, napi_create_double(env, t.co2_night_on, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "co2_night_on", v));
    NAPI_CALL(env, napi_create_double(env, t.co2_night_off, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "co2_night_off", v));

    NAPI_CALL(env, napi_create_double(env, t.ph_min, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "ph_min", v));
    NAPI_CALL(env, napi_create_double(env, t.ph_max, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "ph_max", v));

    NAPI_CALL(env, napi_create_double(env, t.ec_min, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "ec_min", v));
    NAPI_CALL(env, napi_create_double(env, t.ec_max, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "ec_max", v));

    NAPI_CALL(env, napi_create_double(env, t.n_min, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "n_min", v));
    NAPI_CALL(env, napi_create_double(env, t.n_max, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "n_max", v));

    NAPI_CALL(env, napi_create_double(env, t.p_min, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "p_min", v));
    NAPI_CALL(env, napi_create_double(env, t.p_max, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "p_max", v));

    NAPI_CALL(env, napi_create_double(env, t.k_min, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "k_min", v));
    NAPI_CALL(env, napi_create_double(env, t.k_max, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "k_max", v));

    NAPI_CALL(env, napi_create_int32(env, t.fan_speed, &v));
    NAPI_CALL(env, napi_set_named_property(env, obj, "fan_speed", v));

    return obj;
}

static napi_value setAutoControlCommandTopic(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    char buf[256] = {0};
    size_t len = 0;
    if (argc >= 1) {
        napi_valuetype t;
        NAPI_CALL(env, napi_typeof(env, args[0], &t));
        if (t == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], buf, sizeof(buf) - 1, &len));
        }
    }

    control::SetCommandTopic(len > 0 ? buf : nullptr);

    napi_value result;
    NAPI_CALL(env, napi_create_int32(env, 0, &result));
    return result;
}

} // namespace

napi_value RegisterControlApis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("setAutoControlEnabled", setAutoControlEnabled),
        DECLARE_NAPI_FUNCTION("getAutoControlEnabled", getAutoControlEnabled),
        DECLARE_NAPI_FUNCTION("setAutoControlThresholds", setAutoControlThresholds),
        DECLARE_NAPI_FUNCTION("getAutoControlThresholds", getAutoControlThresholds),
        DECLARE_NAPI_FUNCTION("setAutoControlCommandTopic", setAutoControlCommandTopic),
    };

    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
