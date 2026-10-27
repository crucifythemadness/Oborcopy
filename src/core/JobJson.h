#pragma once

#include "JobOptions.h"

#include <QJsonObject>

// JSON (de)serialisation of job profiles. Missing keys fall back to defaults,
// so older job files keep loading when new options are added.
namespace obor::JobJson {

QJsonObject toJson(const JobOptions &job);
JobOptions fromJson(const QJsonObject &object);

// {"label": "BACKUP", "root": "E:/"}; anything else (e.g. a plain string) reads as empty.
QJsonObject toJson(const VolumeRef &ref);
VolumeRef volumeRefFromJson(const QJsonValue &value);

} // namespace obor::JobJson
