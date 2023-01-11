#include "graph_page.h"
#include "dds_data.h"


//------------------------------------------------------------------------------
GraphPage::GraphPage(QWidget* parent) :
    QWidget(parent),
    m_propertiesDialog(NULL),
    m_propertiesUI(NULL),
    m_xAxis(NULL),
    m_picker(NULL),
    m_marker(NULL),
    m_refreshTimer(this),
    m_xCounter(0.0),
    m_xMin(0.0),
    m_xMax(0.0)
{
    setupUi(this);

    m_propertiesDialog = new QDialog(this);
    m_propertiesUI = new Ui::GraphPropertiesPage;
    m_propertiesUI->setupUi(m_propertiesDialog);

    attachButton->hide();
    ejectButton->hide(); // Remove this line after attach/detach is ready

    // Setup the initial x-axis data values;
    for (int i = 0; i < MAX_HISTORY; i++)
    {
        m_xData[i] = MAX_HISTORY - i - 1;
    }
    m_xMin = m_xData[MAX_HISTORY - 1];
    m_xMax = m_xData[MAX_HISTORY - m_propertiesUI->viewSpinBox->value()];


    //--------------------------------------------------------------------------
    // Set the permanent attributes of the qwt plot

    // Add a plot grid
    QwtPlotGrid* grid = new QwtPlotGrid();
    grid->setPen(QColor(0, 0, 0, 50));
    grid->attach(qwtPlot);

    // Qt::WA_PaintOnScreen is only supported for X11, but leads
    // to substantial bugs with Qt 4.2.x/Windows
    #if QT_VERSION >= 0x040000
    #ifdef Q_WS_X11
    qwtPlot->canvas()->setAttribute(Qt::WA_PaintOnScreen, true);
    #endif
    #endif

    // Insert the variable legend at the bottom of the screen
    qwtPlot->insertLegend(new QwtLegend(), QwtPlot::BottomLegend);

    // Create and set the custom label x-axis object
    m_xAxis = new TimeScaleDraw;
    qwtPlot->setAxisScaleDraw(QwtPlot::xBottom, m_xAxis);

    // When the x-axis label is on the far right, we need to prevent the
    // 'jumping canvas' effect
    QwtScaleWidget* xScale = qwtPlot->axisWidget(QwtPlot::xBottom);
    xScale->setMinBorderDist(0, 35);

    qwtPlot->setCanvasBackground(QColor(255, 255, 255, 255));
    qwtPlot->plotLayout()->setAlignCanvasToScales(true);

    // Configure the picker
    m_picker = new QwtPicker(qwtPlot->canvas());
    m_picker->setTrackerMode(QwtPicker::AlwaysOff);
    m_picker->setStateMachine(new QwtPickerClickPointMachine);
    m_picker->setRubberBand(QwtPicker::CrossRubberBand);

    // Configure the clicked point
    QwtSymbol* markerSymbol = new QwtSymbol;
    markerSymbol->setStyle(QwtSymbol::Ellipse);
    markerSymbol->setSize(10, 10);

    m_marker = new QwtPlotMarker();
    m_marker->setSymbol(markerSymbol);
    m_marker->setLabelAlignment(Qt::AlignLeft);
    m_marker->setLineStyle(QwtPlotMarker::NoLine);

    // Pull in the remaining plot attributes from the control widgets
    graphAttributesChanged();
    m_refreshTimer.start();


    //--------------------------------------------------------------------------
    // Connect all Qt signals and slots
    connect(&m_refreshTimer, SIGNAL(timeout()),
        this, SLOT(updateGraph()));
    connect(m_propertiesUI->refreshSpinBox, SIGNAL(valueChanged(int)),
        this, SLOT(graphAttributesChanged()));
    connect(m_propertiesUI->viewSpinBox, SIGNAL(valueChanged(int)),
        this, SLOT(graphAttributesChanged()));
    connect(m_propertiesUI->minYSpinBox, SIGNAL(valueChanged(double)),
        this, SLOT(graphAttributesChanged()));
    connect(m_propertiesUI->maxYSpinBox, SIGNAL(valueChanged(double)),
        this, SLOT(graphAttributesChanged()));
    connect(m_propertiesUI->customYCheckBox, SIGNAL(clicked()),
        this, SLOT(graphAttributesChanged()));
    connect(m_propertiesUI->customXCheckBox, SIGNAL(clicked()),
        this, SLOT(graphAttributesChanged()));
    connect(m_propertiesUI->customXTimeCheckBox, SIGNAL(clicked()),
        this, SLOT(graphAttributesChanged()));
    connect(m_propertiesUI->customXValueCombo, SIGNAL(currentIndexChanged(int)),
        this, SLOT(graphAttributesChanged()));
    connect(m_picker, SIGNAL(appended(const QPoint&)),
        this, SLOT(pointClicked(const QPoint&)));

} // End GraphPage::GraphPage


//------------------------------------------------------------------------------
GraphPage::~GraphPage()
{
    m_refreshTimer.stop();

    PlotData* plot = NULL;
    while (!m_plotData.isEmpty())
    {
        plot = m_plotData.back();
        m_plotData.pop_back();

        if (!plot)
        {
            continue;
        }

        plot->curve->detach();
        delete plot;
    }

    // Deleting the qwtPlot object causes a crash in Windows. Don't delete it
    // and live with a small nonrecurring memory leak.
    qwtPlot->detachItems(0, false);
    qwtPlot->setAutoDelete(false);
    qwtPlot->setParent(0);
    qwtPlot = NULL;

} // End GraphPage::~GraphPage


//------------------------------------------------------------------------------
void GraphPage::addVariable(const QString& topicName,
                            const QString& variableName)
{
    // Make sure we're not already plotting this variable
    for (int i = 0; i < m_plotData.count(); i++)
    {
        if (m_plotData.at(i)->topicName == topicName &&
            m_plotData.at(i)->variableName == variableName)
        {
            return;
        }
    }

    PlotData* newCurve = new PlotData;
    double currentValue;

    variableCombo->addItem(variableName);
    m_propertiesUI->customXValueCombo->addItem(variableName);

    playPauseButton->setEnabled(true);
    rewindButton->setEnabled(true);
    forwardButton->setEnabled(true);
    frontButton->setEnabled(true);
    refreshButton->setEnabled(true);
    scaleButton->setEnabled(true);
    shiftButton->setEnabled(true);
    trashButton->setEnabled(true);
    saveButton->setEnabled(true);
    printButton->setEnabled(true);

    newCurve->topicName = topicName;
    newCurve->variableName = variableName;
    newCurve->curve = new QwtPlotCurve(topicName + "." + variableName);
    newCurve->curve->attach(qwtPlot);
    newCurve->curve->setRenderHint(QwtPlotItem::RenderAntialiased);

    // Fill the data array with the initial value
    currentValue = CommonData::readValue(
        newCurve->topicName, newCurve->variableName).toDouble();

    for (int i = 0; i < MAX_HISTORY; i++)
    {
        newCurve->xData[i] = m_xCounter;
        newCurve->yData[i] = currentValue;
    }

    m_plotData.append(newCurve);

} // End GraphPage::addVariable


//------------------------------------------------------------------------------
void GraphPage::graphAttributesChanged()
{
    // The spinbox is in Hz and the timer is in ms, so convert Hz to ms.
    const int refreshRate = 1000 / m_propertiesUI->refreshSpinBox->value();
    const QString customAxis = m_propertiesUI->customXValueCombo->currentText();
    const PlotData* const axisPlot = getPlot(customAxis);
    const bool customXEnabled = m_propertiesUI->customXCheckBox->isChecked();
    PlotData* plot = NULL;


    //--------------------------------------------------------------------------
    // Enable a custom y scale if it was selected
    if (m_propertiesUI->customYCheckBox->isChecked())
    {
        m_propertiesUI->minYSpinBox->setEnabled(true);
        m_propertiesUI->maxYSpinBox->setEnabled(true);
        qwtPlot->setAxisScale(QwtPlot::yLeft,
            m_propertiesUI->minYSpinBox->value(),
            m_propertiesUI->maxYSpinBox->value());
    }
    else
    {
        m_propertiesUI->minYSpinBox->setEnabled(false);
        m_propertiesUI->maxYSpinBox->setEnabled(false);
        qwtPlot->setAxisAutoScale(QwtPlot::yLeft);
    }


    //--------------------------------------------------------------------------
    // Enable a custom x axis if it was selected
    for (int i = 0; i < m_plotData.count(); i++)
    {
        plot = m_plotData.at(i);
        if (!plot)
        {
            continue;
        }

        // Remove the plot from the x-axis if it's not selected anymore
        if ((plot->isAxisData == true && plot != axisPlot) ||
            (plot->isAxisData == true && customXEnabled == false))
        {
            plot->isAxisData = false;
            plot->curve->attach(qwtPlot);
        }

        // Enable the flag if it was chosen
        if (customXEnabled == true && plot == axisPlot)
        {
            plot->isAxisData = true;
            plot->curve->detach();
        }
    }


    updateGraph();
    m_refreshTimer.setInterval(refreshRate);

} // End GraphPage::graphAttributesChanged


//------------------------------------------------------------------------------
void GraphPage::updateGraph()
{
    // If we only have 1 variable, we don't need the selection combo or
    // trash icon shown
    if (variableCombo->count() <= 1)
    {
        trashButton->setEnabled(false);
        variableCombo->hide();
    }
    else
    {
        trashButton->setEnabled(true);
        variableCombo->show();
    }


    // If we have nothing to plot, we can bail
    if (m_plotData.isEmpty())
    {
        return;
    }

    const int viewSize = m_propertiesUI->viewSpinBox->value();
    PlotData* plot = NULL;
    m_xCounter += 1.0;


    // Shift the x data back one array position
    if (m_xData[0] < m_xCounter)
    {
        for (int c = MAX_HISTORY - 1; c > 0; c--)
        {
            m_xData[c] = m_xData[c - 1];
        }
        m_xData[0] = m_xCounter;
    }


    //--------------------------------------------------------------------------
    // Setup the plot xaxis view
    // Stretch the data to fit the view if the number of ticks is less than
    // the view.
    if (m_xCounter < viewSize)
    {
        m_xMin = m_xData[MAX_HISTORY - 1];
        m_xMax = m_xData[MAX_HISTORY - (int)m_xCounter - 1];
    }

    // If we're at the far right of the plot, show the latest data
    else if (m_xMax + 1.0 == m_xCounter)
    {
        m_xMin = m_xCounter - viewSize + 1.0;
        m_xMax = m_xCounter;
    }

    // If we're at the end of the history and are scrolled to the far right,
    // shift the min/max up
    else if (m_xMin <= m_xData[MAX_HISTORY - 1] &&
        m_xData[MAX_HISTORY - 1] != 0.0)
    {
        m_xMin = m_xData[MAX_HISTORY - 1];
        m_xMax = m_xData[MAX_HISTORY - viewSize];
    }

    // If we are panned left or right, hold the position
    else if (m_xMax + 1.5 < m_xCounter)
    {
        m_xMin = m_xMax - viewSize + 1.0;
    }

    // It should never get here, but if it does, just show the latest data.
    else
    {
        m_xMin = m_xCounter - viewSize + 1.0;
        m_xMax = m_xCounter;
    }


    // Update the x-axis label. If a custom x axis is enabled, the label will
    // be set in the variable loop below.
    if (m_propertiesUI->customXCheckBox->isChecked() == false)
    {
        m_xAxis->addLabel(m_xCounter, QTime::currentTime().toString());
    }


    //--------------------------------------------------------------------------
    // Loop through each variable and attach the latest data
    for (int i = 0; i < m_plotData.count(); i++)
    {
        int arrayStart = 0;
        plot = m_plotData.at(i);
        if (!plot)
        {
            continue;
        }

        // If the latest data isn't valid, skip it
        QVariant latestValue = CommonData::readValue(
            plot->topicName, plot->variableName);
        if (!latestValue.isValid())
        {
            continue;
        }

        // Shift the data back one position
        for (int c = MAX_HISTORY - 1; c > 0; c--)
        {
            plot->xData[c] = plot->xData[c - 1];
            plot->yData[c] = plot->yData[c - 1];
        }


        // Get the starting axis array element
        for (int c = 0; c < MAX_HISTORY; c++)
        {
            if (plot->xData[c] == m_xMax)
            {
                arrayStart = c;
                break;
            }
        }

        // Store the latest value
        plot->xData[0] = m_xCounter;
        plot->yData[0] = latestValue.toDouble();


        // Apply the bias and populate the view arrays
        for (int c = 0; c < viewSize; c++)
        {
            plot->xViewData[c] = plot->xData[c + arrayStart];
            plot->yViewData[c] = plot->yData[c + arrayStart] *
                plot->biasScale +
                plot->biasShift;
        }


        // If this data has been marked as a custom x-axis, set the x-axis
        // label to the value
        if (plot->isAxisData == true)
        {
            const double axisValue = plot->yData[0];

            // If the value is a normalized time, show it as a time
            if (m_propertiesUI->customXTimeCheckBox->isChecked() == true)
            {
                QTime timeValue(0, 0, 0, 0);
                timeValue = timeValue.addSecs((int)axisValue);
                m_xAxis->addLabel(m_xCounter, timeValue.toString());
            }
            else
            {
                m_xAxis->addLabel(m_xCounter, QString::number(axisValue));
            }

            continue;
        }


        // Set the color of the line based on the plot index
        switch (i)
        {
            case 0: plot->color.setRgb(0, 0, 255, 255); break; // Blue
            case 1: plot->color.setRgb(255, 0, 0, 255); break; // Red
            case 2: plot->color.setRgb(0, 200, 0, 255); break; // Green
            case 3: plot->color.setRgb(0, 0, 0, 255); break; // Black
            case 4: plot->color.setRgb(255, 155, 0, 255); break; // Orange
            case 5: plot->color.setRgb(255, 0, 255, 255); break; // Purple
            default: plot->color.setRgb(0, 0, 0, 255); break; // Black
        }

        const QString newTitle =
            plot->topicName + "." + plot->variableName +
            " [" + latestValue.toString() + "]";

        plot->curve->setTitle(newTitle);
        plot->curve->setPen(plot->color);
        plot->curve->setSamples(plot->xViewData,
            plot->yViewData,
            viewSize);

    } // End plot line loop

    qwtPlot->setAxisScale(QwtPlot::xBottom, m_xMin, m_xMax);
    qwtPlot->replot();

} // End GraphPage::updateGraph


//------------------------------------------------------------------------------
void GraphPage::pointClicked(const QPoint& point)
{
    QString variableName = variableCombo->currentText();
    PlotData* plot = getPlot(variableName);

    // If we didn't find the selected variable, bail
    if (plot == NULL)
    {
        return;
    }

    // Figure out the value at the closest clicked point
    int closestGraphPoint = plot->curve->closestPoint(point);
    double xValue = plot->curve->sample(closestGraphPoint).x();
    double yValue = plot->curve->sample(closestGraphPoint).y();

    // Draw the point on the plot
    m_marker->setLabel(QwtText(QString::number(yValue)));
    m_marker->setValue(xValue, yValue);
    m_marker->attach(qwtPlot);

    qwtPlot->replot();

} // End GraphPage::pointClicked


//------------------------------------------------------------------------------
void GraphPage::on_playPauseButton_clicked()
{
    if (m_refreshTimer.isActive())
    {
        m_refreshTimer.stop();
        playPauseButton->setIcon(QIcon(":/images/player-play.png"));
    }
    else
    {
        m_refreshTimer.start(1000 / m_propertiesUI->refreshSpinBox->value());
        playPauseButton->setIcon(QIcon(":/images/player-pause.png"));
    }

} // End GraphPage::on_playPauseButton_clicked


//------------------------------------------------------------------------------
void GraphPage::on_rewindButton_clicked()
{
    const int viewSize = m_propertiesUI->viewSpinBox->value();

    // Make sure we can pan to the left without going past 0
    if (m_xMin - viewSize / 3 > m_xData[MAX_HISTORY - 1])
    {
        m_xMin -= viewSize / 3;
        m_xMax -= viewSize / 3;
    }
    else
    {
        m_xMin = m_xData[MAX_HISTORY - 1];
        m_xMax = m_xData[MAX_HISTORY - viewSize];
    }

    qwtPlot->replot();
}


//------------------------------------------------------------------------------
void GraphPage::on_forwardButton_clicked()
{
    const int viewSize = m_propertiesUI->viewSpinBox->value();

    // Make sure we can pan to the right without going past the max
    if (m_xMax + viewSize / 3 < m_xData[0])
    {
        m_xMin += viewSize / 3;
        m_xMax += viewSize / 3;
    }
    else
    {
        m_xMin = m_xData[viewSize];
        m_xMax = m_xData[0];
    }

    qwtPlot->replot();
}


//------------------------------------------------------------------------------
void GraphPage::on_frontButton_clicked()
{
    m_xMin = m_xData[m_propertiesUI->viewSpinBox->value()];
    m_xMax = m_xData[0];

    qwtPlot->replot();
}


//------------------------------------------------------------------------------
void GraphPage::on_refreshButton_clicked()
{
    PlotData* plot = NULL;
    double currentValue;

    // Setup the initial x-axis data values;
    for (int i = 0; i < MAX_HISTORY; i++)
    {
        m_xData[i] = MAX_HISTORY - i - 1;
    }

    m_xCounter = 0.0;
    m_xMin = m_xData[MAX_HISTORY - 1];
    m_xMax = m_xData[MAX_HISTORY - m_propertiesUI->viewSpinBox->value()];
    m_marker->detach();

    // Clear out the label history
    while (m_xAxis != NULL && !m_xAxis->m_xLabels.isEmpty())
    {
        delete m_xAxis->m_xLabels.back();
        m_xAxis->m_xLabels.pop_back();
    }


    // Reset the data for each of the plots
    for (int i = 0; i < m_plotData.count(); i++)
    {
        plot = m_plotData.at(i);
        if (plot == NULL)
        {
            continue;
        }

        // Flush all the data back to 0
        memset(plot->xData, 0, sizeof(plot->xData));
        memset(plot->yData, 0, sizeof(plot->yData));
        memset(plot->xViewData, 0, sizeof(plot->xViewData));
        memset(plot->yViewData, 0, sizeof(plot->yViewData));

        // Fill the data array with the current value
        currentValue = CommonData::readValue(
            plot->topicName, plot->variableName).toDouble();

        for (int j = 0; j < MAX_HISTORY; j++)
        {
            plot->xData[j] = m_xCounter;
            plot->yData[j] = currentValue;
        }

    } // End plot loop

    updateGraph();

} // End GraphPage::on_refreshButton_clicked


//------------------------------------------------------------------------------
void GraphPage::on_scaleButton_clicked()
{
    QString variableName = variableCombo->currentText();
    bool ok;

    PlotData* plotData = getPlot(variableName);
    if (plotData == NULL)
    {
        return;
    }

    // Set the new scaler value
    double value = QInputDialog::getDouble(this,
        "Variable Scaler",
        "Scale " + variableName + " by:",
        plotData->biasScale,
        - 1000.0, 1000.0, 5, &ok);

    if (ok)
    {
        plotData->biasScale = value;
        updateGraph();
    }

} // End GraphPage::on_scaleButton_clicked


//------------------------------------------------------------------------------
void GraphPage::on_shiftButton_clicked()
{
    QString variableName = variableCombo->currentText();
    bool ok;

    PlotData* plotData = getPlot(variableName);
    if (plotData == NULL)
    {
        return;
    }

    // Set the new shift value
    double value = QInputDialog::getDouble(this,
        "Variable Shift",
        "Shift " + variableName + " by:",
        plotData->biasShift,
        - 10000.0, 10000.0, 5, &ok);

    if (ok)
    {
        plotData->biasShift = value;
        updateGraph();
    }

} // End GraphPage::on_shiftButton_clicked


//------------------------------------------------------------------------------
void GraphPage::on_configButton_clicked()
{
    m_propertiesDialog->exec();
}


//------------------------------------------------------------------------------
void GraphPage::on_saveButton_clicked()
{
    const QSize startSize = qwtPlot->size();
    const QSize imageSize(800, 600);
    QPixmap image(imageSize);

    qwtPlot->resize(imageSize);
    qwtPlot->render(&image);
    qwtPlot->resize(startSize);

    QString filePath;
    filePath = QFileDialog::getSaveFileName(this,
        "Save Graph", "", "PNG Image Files (*.png)");

    if (!filePath.isEmpty())
    {
        image.save(filePath, "PNG");
    }
}


//------------------------------------------------------------------------------
void GraphPage::on_printButton_clicked()
{
    QPrinter printer /*(QPrinter::HighResolution)*/;
    printer.setPageOrientation(QPageLayout::Landscape);

    QPrintDialog dialog(&printer);
    if (dialog.exec())
    {
        QwtPlotRenderer renderer(this);
        renderer.renderTo(qwtPlot, printer);
    }
}


//------------------------------------------------------------------------------
void GraphPage::on_trashButton_clicked()
{
    // Bail if nothing was selected
    if (variableCombo->currentText().isEmpty())
    {
        return;
    }

    PlotData* plot = NULL;

    // Remove the selected variable in the data list
    for (int i = 0; i < m_plotData.count(); i++)
    {
        plot = m_plotData.at(i);

        if (!plot || plot->variableName != variableCombo->currentText())
        {
            continue;
        }

        m_plotData.removeAt(i);
        plot->curve->detach();
        delete plot;

        plot = NULL;
        break;
    }

    // Reset the custom x-axis control if they removed the x-axis variable
    if (m_propertiesUI->customXValueCombo->currentIndex() ==
        variableCombo->currentIndex())
    {
        m_propertiesUI->customXCheckBox->setChecked(false);
    }

    // Pull the variable out of the combo boxes
    m_propertiesUI->customXValueCombo->removeItem(variableCombo->currentIndex());
    variableCombo->removeItem(variableCombo->currentIndex());

    // Disable the control buttons and clear the graph if nothing is left
    if (variableCombo->count() < 1)
    {
        playPauseButton->setEnabled(false);
        rewindButton->setEnabled(false);
        forwardButton->setEnabled(false);
        frontButton->setEnabled(false);
        refreshButton->setEnabled(false);
        scaleButton->setEnabled(false);
        shiftButton->setEnabled(false);
        trashButton->setEnabled(false);
        saveButton->setEnabled(false);
        printButton->setEnabled(false);

        // Flush everything out of the graph
        on_refreshButton_clicked();
        qwtPlot->close();
        qwtPlot->replot();
    }

} // End GraphPage::on_trashButton_clicked


//------------------------------------------------------------------------------
void GraphPage::on_ejectButton_clicked()
{
    ejectButton->hide();
    attachButton->show();

    emit detachWidget(this->parentWidget());
}


//------------------------------------------------------------------------------
void GraphPage::on_attachButton_clicked()
{
    attachButton->hide();
    ejectButton->show();

    emit attachWidget(this->parentWidget());
}


//------------------------------------------------------------------------------
GraphPage::PlotData* GraphPage::getPlot(const QString& variableName)
{
    PlotData* plot = NULL;

    for (int i = 0; i < m_plotData.count(); i++)
    {
        plot = m_plotData.at(i);
        if (!plot)
        {
            continue;
        }

        // Is this the plot we're looking for?
        if (plot->variableName == variableName)
        {
            return plot;
        }
    }

    return NULL;
}


//------------------------------------------------------------------------------
GraphPage::TimeScaleDraw::~TimeScaleDraw()
{}


//------------------------------------------------------------------------------
void GraphPage::TimeScaleDraw::addLabel(const double& value,
                                        const QString& label)
{
    // Make sure we don't have too many x-labels
    while (m_xLabels.count() > MAX_HISTORY)
    {
        delete m_xLabels.front();
        m_xLabels.pop_front();
    }

    QPair<double, QString>* labelPair = new QPair<double, QString>;

    labelPair->first = value;
    labelPair->second = label;

    m_xLabels.push_back(labelPair);
}


//------------------------------------------------------------------------------
QwtText GraphPage::TimeScaleDraw::label(double value) const
{
    for (int i = 0; i < m_xLabels.count(); i++)
    {
        if (!m_xLabels.at(i))
        {
            continue;
        }

        if (m_xLabels.at(i)->first == value)
        {
            return m_xLabels.at(i)->second;
        }
    }

    return QwtText("");
}


//------------------------------------------------------------------------------
GraphPage::PlotData::PlotData()
{
    topicName = "-";
    variableName = "-";
    biasScale = 1.0;
    biasShift = 0.0;
    curve = NULL;

    memset(xData, 0, sizeof(xData));
    memset(yData, 0, sizeof(yData));
    memset(xViewData, 0, sizeof(xViewData));
    memset(yViewData, 0, sizeof(yViewData));

    isAxisData = false;
}


//------------------------------------------------------------------------------
GraphPage::PlotData::~PlotData()
{
    curve = NULL;
}


/**
 * @}
 */
