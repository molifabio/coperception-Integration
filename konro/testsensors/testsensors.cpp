#include <iostream>
#include <sensors.h>

int main()
{
    sensors_init(NULL);

    sensors_chip_name const * cn;
    int c = 0;
    while ((cn = sensors_get_detected_chips(0, &c)) != 0) {
        std::cout << "Chip: " << cn->prefix << " - " << cn->path << std::endl;

        sensors_feature const *feat;
        int f = 0;

        while ((feat = sensors_get_features(cn, &f)) != 0) {
#if 0
            if (feat->type != SENSORS_FEATURE_TEMP)
                continue;
#endif
            std::cout << f << ": feature name is " << feat->name << std::endl;
            std::cout << f << ": feature label is " << sensors_get_label(cn, feat) << std::endl;
            std::cout << f << ": feature type is " << feat->type << std::endl;

            sensors_subfeature const *subf;
            int s = 0;

            while ((subf = sensors_get_all_subfeatures(cn, feat, &s)) != 0) {
                std::cout << f << ":" << s << ": subfeature type is " << subf->type << std::endl;
                std::cout << f << ":" << s << ": subfeature name is " << subf->name
                          << "/" << subf->number << " = ";
                double val;
                if (subf->flags & SENSORS_MODE_R) {
                    int rc = sensors_get_value(cn, subf->number, &val);
                    if (rc < 0) {
                        std::cout << "err: " << rc;
                    } else {
                        std::cout << val;
                    }
                }
                std::cout << std::endl;
            }
        }
    }

    sensors_cleanup();
}
