/*---------------------------------------------------------*\
| OpenRGBZoneEditorDialog.cpp                               |
|                                                           |
|   User interface for editing zones                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <fstream>
#include <QComboBox>
#include <QFileDialog>
#include <QLineEdit>
#include "OpenRGBZoneEditorDialog.h"
#include "ui_OpenRGBZoneEditorDialog.h"

OpenRGBZoneEditorDialog::OpenRGBZoneEditorDialog(RGBController* edit_dev_ptr, unsigned int edit_zone_idx_val, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OpenRGBZoneEditorDialog)
{
    edit_dev      = edit_dev_ptr;
    edit_zone_idx = edit_zone_idx_val;

    unsigned int size_min     = edit_dev->GetZoneLEDsMin(edit_zone_idx);
    unsigned int size_max     = edit_dev->GetZoneLEDsMax(edit_zone_idx);
    unsigned int size_current = edit_dev->GetZoneLEDsCount(edit_zone_idx);

    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QStringList header_labels;
    header_labels << "Name" << "Type" << "Size" << "";
    ui->SegmentsTreeWidget->setHeaderLabels(header_labels);

    ui->ResizeSlider->setRange(size_min, size_max);
    ui->ResizeBox->setRange(size_min, size_max);

    ui->ResizeSlider->setValue(size_current);
    ui->ResizeBox->setValue(size_current);

    for(unsigned int segment_idx = 0; segment_idx < edit_dev->GetZoneSegmentCount(edit_zone_idx); segment_idx++)
    {
        AddSegmentRow(QString::fromStdString(edit_dev->GetZoneSegmentName(edit_zone_idx, segment_idx)), edit_dev->GetZoneSegmentLEDsCount(edit_zone_idx, segment_idx), edit_dev->GetZoneSegmentType(edit_zone_idx, segment_idx));
    }
}

OpenRGBZoneEditorDialog::~OpenRGBZoneEditorDialog()
{
    delete ui;
}

void OpenRGBZoneEditorDialog::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

void OpenRGBZoneEditorDialog::on_ResizeSlider_valueChanged(int value)
{
    ui->ResizeBox->blockSignals(true);
    ui->ResizeBox->setValue(value);
    ui->ResizeBox->blockSignals(false);

    /*-----------------------------------------------------*\
    | Set maximum value for all segment sliders to new zone |
    | size                                                  |
    \*-----------------------------------------------------*/
    for(int item_idx = 0; item_idx < ui->SegmentsTreeWidget->topLevelItemCount(); item_idx++)
    {
        ((QSlider*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 3))->setMaximum(value);
    }

    CheckSegmentsValidity();
}

void OpenRGBZoneEditorDialog::on_segment_lineedit_textChanged()
{
    /*-----------------------------------------------------*\
    | Update the Slider with the LineEdit value for each    |
    | segment                                               |
    \*-----------------------------------------------------*/
    for(int item_idx = 0; item_idx < ui->SegmentsTreeWidget->topLevelItemCount(); item_idx++)
    {
        int lineedit_value = ((QLineEdit*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 2))->text().toInt();
        ((QSlider*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 3))->setValue(lineedit_value);
    }

    CheckSegmentsValidity();
}

void OpenRGBZoneEditorDialog::on_segment_slider_valueChanged(int)
{
    /*-----------------------------------------------------*\
    | Update the LineEdit with the Slider value for each    |
    | segment                                               |
    \*-----------------------------------------------------*/
    for(int item_idx = 0; item_idx < ui->SegmentsTreeWidget->topLevelItemCount(); item_idx++)
    {
        int slider_value = ((QSlider*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 3))->value();
        ((QLineEdit*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 2))->setText(QString::number(slider_value));
    }

    CheckSegmentsValidity();
}

void OpenRGBZoneEditorDialog::on_ResizeBox_valueChanged(int value)
{
    ui->ResizeSlider->blockSignals(true);
    ui->ResizeSlider->setValue(value);
    ui->ResizeSlider->blockSignals(false);

    /*-----------------------------------------------------*\
    | Set maximum value for all segment sliders to new zone |
    | size                                                  |
    \*-----------------------------------------------------*/
    for(int item_idx = 0; item_idx < ui->SegmentsTreeWidget->topLevelItemCount(); item_idx++)
    {
        ((QSlider*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 3))->setMaximum(value);
    }

    CheckSegmentsValidity();
}

int OpenRGBZoneEditorDialog::show()
{
    int ret_val = 0;

    int result = this->exec();

    if(result == QDialog::Rejected)
    {
        ret_val = -1;
    }
    else
    {
        ret_val = ui->ResizeBox->value();
    }

    if(ret_val >= 0 && edit_dev != NULL)
    {
        edit_dev->ResizeZone(edit_zone_idx, ret_val);

        edit_dev->ClearSegments(edit_zone_idx);

        unsigned int start_idx = 0;

        for(int item_idx = 0; item_idx < ui->SegmentsTreeWidget->topLevelItemCount(); item_idx++)
        {
            segment new_segment;
            new_segment.type       = ((QComboBox*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 1))->currentIndex();
            new_segment.name       = ((QLineEdit*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 0))->text().toStdString();
            new_segment.start_idx  = start_idx;
            new_segment.leds_count = ((QLineEdit*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(item_idx), 2))->text().toInt();

            edit_dev->AddSegment(edit_zone_idx, new_segment);

            start_idx += new_segment.leds_count;
        }
    }

    return(ret_val);
}

void OpenRGBZoneEditorDialog::AddSegmentRow(QString name, unsigned int length, zone_type type)
{
    /*---------------------------------------------------------*\
    | Create new line in segments list tree                     |
    \*---------------------------------------------------------*/
    QTreeWidgetItem* new_item   = new QTreeWidgetItem(ui->SegmentsTreeWidget);

    /*---------------------------------------------------------*\
    | Create new widgets for line                               |
    \*---------------------------------------------------------*/
    QComboBox* combobox_type    = new QComboBox(ui->SegmentsTreeWidget);
    QLineEdit* lineedit_name    = new QLineEdit(ui->SegmentsTreeWidget);
    QLineEdit* lineedit_length  = new QLineEdit(ui->SegmentsTreeWidget);
    QSlider*   slider_length    = new QSlider(Qt::Horizontal, ui->SegmentsTreeWidget);

    /*---------------------------------------------------------*\
    | Fill in Name field                                        |
    \*---------------------------------------------------------*/
    lineedit_name->setText(name);

    /*---------------------------------------------------------*\
    | Set up segment type combo box                             |
    \*---------------------------------------------------------*/
    combobox_type->addItem("Single");
    combobox_type->addItem("Linear");
    combobox_type->addItem("Matrix");
    combobox_type->addItem("Linear Loop");
    combobox_type->addItem("Matrix Loop X");
    combobox_type->addItem("Matrix Loop Y");
    combobox_type->addItem("Segmented");

    combobox_type->setCurrentIndex(type);

    /*---------------------------------------------------------*\
    | Fill in Length field                                      |
    \*---------------------------------------------------------*/
    lineedit_length->setText(QString::number(length));

    /*---------------------------------------------------------*\
    | Fill in slider length and maximum                         |
    \*---------------------------------------------------------*/
    slider_length->setMaximum(edit_dev->GetZoneLEDsCount(edit_zone_idx));
    slider_length->setValue(length);

    /*---------------------------------------------------------*\
    | Add new widgets to tree                                   |
    \*---------------------------------------------------------*/
    ui->SegmentsTreeWidget->setItemWidget(new_item, 0, lineedit_name);
    ui->SegmentsTreeWidget->setItemWidget(new_item, 1, combobox_type);
    ui->SegmentsTreeWidget->setItemWidget(new_item, 2, lineedit_length);
    ui->SegmentsTreeWidget->setItemWidget(new_item, 3, slider_length);

    /*---------------------------------------------------------*\
    | Connect signals for handling slider and line edits        |
    \*---------------------------------------------------------*/
    connect(lineedit_name, &QLineEdit::textChanged, this, &OpenRGBZoneEditorDialog::on_segment_lineedit_textChanged);
    connect(slider_length, &QSlider::valueChanged, this, &OpenRGBZoneEditorDialog::on_segment_slider_valueChanged);
    connect(lineedit_length, &QLineEdit::textChanged, this, &OpenRGBZoneEditorDialog::on_segment_lineedit_textChanged);
}

void OpenRGBZoneEditorDialog::on_AddSegmentButton_clicked()
{
    /*---------------------------------------------------------*\
    | Create new empty row with name "Segment X"                |
    \*---------------------------------------------------------*/
    QString new_name = "Segment " + QString::number(ui->SegmentsTreeWidget->topLevelItemCount() + 1);

    AddSegmentRow(new_name, 0, ZONE_TYPE_LINEAR);

    CheckSegmentsValidity();
}

void OpenRGBZoneEditorDialog::CheckSegmentsValidity()
{
    bool segments_valid = true;

    /*---------------------------------------------------------*\
    | Only check validity if segments are configured            |
    \*---------------------------------------------------------*/
    if(ui->SegmentsTreeWidget->topLevelItemCount() != 0)
    {
        /*-----------------------------------------------------*\
        | Verify all segments add up to zone size               |
        \*-----------------------------------------------------*/
        int total_segment_leds = 0;

        for(int segment_idx = 0; segment_idx < ui->SegmentsTreeWidget->topLevelItemCount(); segment_idx++)
        {
            unsigned int segment_leds = ((QLineEdit*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(segment_idx), 2))->text().toInt();

            /*-------------------------------------------------*\
            | Zero-length segment is not allowed                |
            \*-------------------------------------------------*/
            if(segment_leds == 0)
            {
                segments_valid = false;
            }

            total_segment_leds += segment_leds;

            /*-------------------------------------------------*\
            | Empty name is not allowed                         |
            \*-------------------------------------------------*/
            if(((QLineEdit*)ui->SegmentsTreeWidget->itemWidget(ui->SegmentsTreeWidget->topLevelItem(segment_idx), 0))->text().isEmpty())
            {
                segments_valid = false;
            }
        }

        if(total_segment_leds != ui->ResizeBox->value())
        {
            segments_valid = false;
        }
    }

    ui->ButtonBox->setEnabled(segments_valid);
}

void OpenRGBZoneEditorDialog::on_RemoveSegmentButton_clicked()
{
    ui->SegmentsTreeWidget->takeTopLevelItem(ui->SegmentsTreeWidget->topLevelItemCount() - 1);

    CheckSegmentsValidity();
}

void OpenRGBZoneEditorDialog::on_ImportConfigurationButton_clicked()
{
    QFileDialog file_dialog(this);

    file_dialog.setFileMode(QFileDialog::ExistingFile);
    file_dialog.setNameFilter("*.json");
    file_dialog.setWindowTitle("Import Configuration");

    if(file_dialog.exec())
    {
        QString         filename        = file_dialog.selectedFiles()[0];
        std::ifstream   config_file(filename.toStdString(), std::ios::in);
        nlohmann::json  config_json;

        if(config_file)
        {
            try
            {
                config_file >> config_json;

                if(config_json.contains("segments"))
                {
                    unsigned int total_leds_count = ui->ResizeSlider->value();

                    for(std::size_t segment_idx = 0; segment_idx < config_json["segments"].size(); segment_idx++)
                    {
                        unsigned int    segment_leds_count  = 0;
                        matrix_map_type segment_matrix_map;
                        QString         segment_name        = "";
                        zone_type       segment_type        = ZONE_TYPE_LINEAR;

                        if(config_json["segments"][segment_idx].contains("name"))
                        {
                            segment_name = QString::fromStdString(config_json["segments"][segment_idx]["name"]);
                        }
                        if(config_json["segments"][segment_idx].contains("leds_count"))
                        {
                            segment_leds_count = config_json["segments"][segment_idx]["leds_count"];
                        }
                        if(config_json["segments"][segment_idx].contains("type"))
                        {
                            segment_type = config_json["segments"][segment_idx]["type"];
                        }

                        AddSegmentRow(segment_name, segment_leds_count, segment_type);

                        total_leds_count += segment_leds_count;
                    }


                    ui->ResizeSlider->setValue(total_leds_count);
                    ui->ResizeBox->setValue(total_leds_count);
                }
            }
            catch(const std::exception& e)
            {
            }
        }
    }
}
