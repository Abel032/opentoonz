#pragma once
#ifndef FramesDIALOG_H
#define FramesDIALOG_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include <QList>
#include <QStringList>
#include <QVariant>
#include <QGroupBox>
#include <QtGlobal>
#include <algorithm>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;

namespace DVGui {
class FileField;
class DoubleLineEdit;
class ColorField;
}  // namespace DVGui

struct FramesDef {
  QList<double> m_params;  // [width, height, r, g, b]

  QString toString() const {
    QStringList parts;
    for (int i = 0; i < m_params.size(); ++i) {
      if (i < 2)  // Width/Height (Percentage)
        parts << QString::number(m_params[i], 'f', 2);
      else  // R, G, B (Integer 0-255)
        parts << QString::number(m_params[i], 'f', 0);
    }
    return parts.join(", ");
  }

  static FramesDef fromString(const QString& str) {
    FramesDef def;
    QStringList parts = str.split(',');
    for (const QString& part : parts) {
      bool ok;
      double val = part.trimmed().toDouble(&ok);
      if (ok) def.m_params.append(val);
    }
    return def;
  }

  static FramesDef fromVariantList(const QList<QVariant>& list) {
    FramesDef def;
    for (const QVariant& v : list) def.m_params.append(v.toDouble());
    return def;
  }

  QList<QVariant> toVariantList() const {
    QList<QVariant> list;
    for (double p : m_params) list.append(p);
    return list;
  }
};

struct FramesPreset {
  QString m_name;
  QString m_layoutPath;
  double m_layoutOffsetX = 0;
  double m_layoutOffsetY = 0;
  QList<FramesDef> m_areas;
};

class FramesPresetsEditorPopup : public DVGui::Dialog {
  Q_OBJECT

private:
  static FramesPresetsEditorPopup* s_instance;
  explicit FramesPresetsEditorPopup(QWidget* parent = nullptr);

public:
  static FramesPresetsEditorPopup* instance(QWidget* parent = nullptr);
  void showAndPosition(const QPoint& globalPos);

protected:
  void accept() override;
  void reject() override;
  void closeEvent(QCloseEvent* e) override;

private:
  TFilePath m_iniFilePath;
  QList<FramesPreset> m_presets;

  QString m_oldFramesName;
  QList<FramesPreset> m_initialPresets;
  bool m_previewActive = false;

  // UI
  QListWidget* m_presetList;
  QGroupBox* m_editorGroup;
  QLineEdit* m_nameEdit;
  DVGui::FileField* m_layoutField;
  DVGui::DoubleLineEdit* m_offsetXEdit;
  DVGui::DoubleLineEdit* m_offsetYEdit;
  QListWidget* m_areaList;

  QGroupBox* m_areaEditorGroup;
  DVGui::DoubleLineEdit* m_widthEdit;
  DVGui::DoubleLineEdit* m_heightEdit;
  DVGui::ColorField* m_colorField;

  QPushButton* m_addAreaBtn;
  QPushButton* m_removeAreaBtn;

  void loadIniFile();
  void saveIniFile();
  void updatePresetList();
  void updateAreaList();
  void updateEditorWidgets();

  QString getCurrentSceneFramesName() const;
  void setSceneFramesPresetName(const QString& name, bool preview);
  void triggerPreview();

private slots:
  void onPresetSelected();
  void onAreaSelected();

  void onAddPreset();
  void onRemovePreset();

  void onMovePresetUp();
  void onMovePresetDown();

  void onAddArea();
  void onRemoveArea();

  void onNameChanged(const QString& text);
  void onLayoutPathChanged();

  void onOffsetXChanged();
  void onOffsetYChanged();

  // Width/Height/RGB changed
  void onAreaParamChanged();
};

#endif  // FramesDIALOG_H