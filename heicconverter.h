#pragma once
#include <QString>

class HeicConverter {
public:
    HeicConverter() = default;
    bool convertFile(const QString &inputFile, const QString &outputFile, int quality = 100);
};
