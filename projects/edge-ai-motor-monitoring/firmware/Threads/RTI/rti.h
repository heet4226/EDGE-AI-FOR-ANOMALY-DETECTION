#ifndef RTI_H
#define RTI_H
#include <stdint.h>
/* Experimental scheduling logic, not validated maintenance advice. */
typedef struct {
    double operating_h;
    double deadline_h;
    double risk_ema;
    uint8_t inspection_due;
    uint8_t initialized;
} RTI_State;

/* Initialize ONLY for a genuinely new inspection cycle. Restore saved state after boot instead. */
void RTI_NewCycle(RTI_State *s);
/* dt_h must be elapsed RUNNING time; risk is [0,1]. Call once per real elapsed interval. */
void RTI_Update(RTI_State *s, double dt_h, double raw_risk);
double RTI_Remaining(const RTI_State *s);
/* Anomaly probability 0..100, class confidence 0..100, mse and positive recon threshold,
   severity ratio >=0, final anomaly decision. PWM-aware inputs can be added after calibration. */
double RTI_RiskFromPrediction(
    double aprob_pct,
    double mse,
    double recon_threshold,
    double severity_ratio,
    double class_conf_pct,
    int known_fault,
    int anomalous);

#endif
