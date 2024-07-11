void worldspawn_deserialize(worldspawn_t *dst, map_t *map, map_entity_t *src)
{
    if (expect_class(map, src, S("worldspawn")))
    {
        dst->sun_color                = v3_from_key_or   (map, src, S("sun_color"), make_v3(1, 1, 1));
        dst->sun_brightness           = float_from_key_or(map, src, S("sun_brightness"), 1.0f);
        dst->fog_absorption           = float_from_key_or(map, src, S("fog_absorption"), 0.002f);
        dst->fog_density              = float_from_key_or(map, src, S("fog_density"), 0.02f);
        dst->fog_scattering           = float_from_key_or(map, src, S("fog_scattering"), 0.04f);
        dst->fog_phase                = float_from_key_or(map, src, S("fog_phase"), 0.6f);
        dst->fog_ambient_inscattering = v3_from_key_or   (map, src, S("fog_ambient_inscattering"), make_v3(0.0f, 0.0f, 0.0f));
    }
}
