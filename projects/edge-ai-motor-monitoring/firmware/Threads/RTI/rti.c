#include "rti.h"
#include <math.h>
#include <stddef.h>

/* Experimental RTI parameters */
#define RTI_BASE_H          100.0
#define RTI_RECOVERY_RATE     0.25
#define RTI_PENALTY_RATE      2.0
#define RTI_FILTER_TAU_H     0.05  /* 3 minutes */

static double bound(double x, double lo, double hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

void RTI_NewCycle(RTI_State *s)
{
    if (!s) return;

    *s = (RTI_State){
        .operating_h = 0.0,
        .deadline_h = RTI_BASE_H,
        .risk_ema = 0.0,
        .inspection_due = 0,
        .initialized = 1
    };
}

/* Input: anomaly and class confidence in 0..100 percent */
double RTI_RiskFromPrediction(
    double aprob_pct,
    double mse,
    double threshold,
    double severity,
    double class_conf_pct,
    int known_fault,
    int anomalous)
{
    if (!isfinite(aprob_pct) ||
        !isfinite(mse) ||
        !isfinite(threshold) ||
        !isfinite(severity) ||
        !isfinite(class_conf_pct) ||
        threshold <= 0.0)
    {
        return 1.0;
    }

    double p_excess = bound(
        (aprob_pct - 50.0) / 50.0, 0.0, 1.0);

    double mse_excess = bound(
        mse / threshold - 1.0, 0.0, 1.0);

    double sev_excess = bound(
        (severity - 1.0) / 3.0, 0.0, 1.0);

    /* Confidence contributes only for a predicted known fault */
    double conf_risk = known_fault
        ? bound(class_conf_pct / 100.0, 0.0, 1.0)
        : 0.0;

    double score =
        0.35 * p_excess +
        0.30 * mse_excess +
        0.25 * sev_excess +
        0.10 * conf_risk;

    /* Retain the anomaly minimum-risk rule */
    if (anomalous && score < 0.60)
        score = 0.60;

    return bound(score, 0.0, 1.0);
}

void RTI_Update(RTI_State *s, double dt_h, double risk)
{
    if (!s || !s->initialized || s->inspection_due)
        return;

    if (!isfinite(dt_h) || dt_h <= 0.0)
        return;

    if (!isfinite(risk))
        risk = 1.0;

    risk = bound(risk, 0.0, 1.0);

    double budget = RTI_BASE_H - s->operating_h;

    if (dt_h > budget)
        dt_h = budget;

    if (dt_h <= 0.0)
    {
        s->inspection_due = 1;
        return;
    }

    /* 3-minute EMA, updated at every RTI service interval */
    double alpha = -expm1(-dt_h / RTI_FILTER_TAU_H);

    s->risk_ema += alpha * (risk - s->risk_ema);

    s->operating_h += dt_h;

    s->deadline_h += dt_h * (
        RTI_RECOVERY_RATE * (1.0 - s->risk_ema)
        - RTI_PENALTY_RATE * s->risk_ema
    );

    /* Never exceed the original 100-hour deadline */
    s->deadline_h = bound(
        s->deadline_h,
        s->operating_h,
        RTI_BASE_H
    );

    if (s->operating_h >= s->deadline_h - 1.0e-9 ||
        s->operating_h >= RTI_BASE_H - 1.0e-9)
    {
        s->inspection_due = 1;
    }
}

double RTI_Remaining(const RTI_State *s)
{
    if (!s || !s->initialized || s->inspection_due)
        return 0.0;

    return bound(
        s->deadline_h - s->operating_h,
        0.0,
        RTI_BASE_H - s->operating_h
    );
}
