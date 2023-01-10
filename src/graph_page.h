#ifndef DEF_VARIABLE_GRAPH_WIDGET
#define DEF_VARIABLE_GRAPH_WIDGET

#include "first_define.h"

#define _USE_MATH_DEFINES 1
#include <cmath>

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QPrintDialog>
#include <QInputDialog>
#include <QFileDialog>
#include <QDataStream>
#include <QDropEvent>
#include <QByteArray>
#include <QPrinter>
#include <QString>
#include <QPixmap>
#include <QWidget>
#include <QColor>
#include <QTimer>
#include <QPoint>
#include <QTime>
#include <QList>
#include <QIcon>
#include <QPair>

#ifdef __GNUG__
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <qwt_picker_machine.h>
#include <qwt_plot_renderer.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_marker.h>
#include <qwt_scale_draw.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_painter.h>
#include <qwt_legend.h>
#include <qwt_symbol.h>
#include <qwt_picker.h>
#include <qwt_plot.h>
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

#include "ui_graph_page.h"
#include "ui_graph_properties.h"


//------------------------------------------------------------------------------
// class GraphPage
//------------------------------------------------------------------------------
/**
 * @brief The variable graphing widget class.
 *
 * @remarks This class uses the QWT graphing library to display the plot.
 * @remarks The icons on the bottom of the page are from
 *          http://www.axialis.com/free/icons.
 *
 */
class GraphPage : public QWidget, public Ui::GraphPage
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for GraphPage.
     * @param[in] parent The parent of this Qt object.
     */
    GraphPage(QWidget* parent = 0);

    /**
     * @brief Destructor for GraphPage.
     */
    ~GraphPage();

    /**
     * @brief Add a new variable to the plotter.
     * @param[in] topicName The DDS topic name.
     * @param[in] variableName The DDS topic member name.
     */
    void addVariable(const QString& topicName, const QString& variableName);

private slots:

    /**
     * @brief Set the attributes of the graph based on the control widgets.
     */
    void graphAttributesChanged();

    /**
     * @brief Plot in the latest value for each variable.
     */
    void updateGraph();

    /**
     * @brief Display the marker on the clicked point for the selected variable.
     * @param[in] point Display the marker at this clicked point.
     */
    void pointClicked(const QPoint& point);

    /**
     * @brief Pause or resume updating the graph.
     */
    void on_playPauseButton_clicked();

    /**
     * @brief Shift the view of the plot back to previous data.
     */
    void on_rewindButton_clicked();

    /**
     * @brief Shift the view of the plot back to new data.
     */
    void on_forwardButton_clicked();

    /**
     * @brief Shift the view of the plot back to the latest data.
     */
    void on_frontButton_clicked();

    /**
     * @brief Clears the plot data.
     */
    void on_refreshButton_clicked();

    /**
     * @brief Sets the scale value for the selected variable.
     */
    void on_scaleButton_clicked();

    /**
     * @brief Sets the scale value for the selected variable.
     */
    void on_shiftButton_clicked();

    /**
     * @brief Displays the graph properties dialog, m_graphProperties.
     */
    void on_configButton_clicked();

    /**
     * @brief Saves the plot to a file.
     */
    void on_saveButton_clicked();

    /**
     * @brief Prints a hard-copy of the plot.
     */
    void on_printButton_clicked();

    /**
     * @brief Removes the selected variable from the plot.
     */
    void on_trashButton_clicked();

    /**
     * @brief Prepares this dialog to be removed from the tab widget.
     */
    void on_ejectButton_clicked();

    /**
     * @brief Prepares this dialog to be attached tab widget.
     */
    void on_attachButton_clicked();

signals:

    /**
     * @brief Notify the main window we should attach to the tab widget.
     */
    void attachWidget(QWidget*);

    /**
     * @brief Notify the main window we should detach to the tab widget.
     */
    void detachWidget(QWidget*);

private:

    /// The maximum number of data points for a variable.
    static const int MAX_HISTORY = 4096;

    /// The maximum number of variable lines to plot.
    static const int MAX_LINES = 6;

    /**
     * @brief Axis class that GraphPage uses to display the custom labels
     *        along the xaxis.
     */
    class TimeScaleDraw : public QwtScaleDraw
    {
    public:

        ///  Contructor for the custom x-axis object.
        TimeScaleDraw() {}

        ///  Destructor for the custom x-axis object.
        ~TimeScaleDraw();

        /**
         * @brief Add a label value to the xaxis.
         * @param[in] value Set the label for this value.
         * @param[in] label The label to display for the given value.
         */
        void addLabel(const double& value, const QString& label);

        /**
         * @brief Return the label string for the passed in value.
         * @remarks Reimplemented from QwtAbstractScaleDraw.
         * @param[in] value Return the label for with this value.
         * @return The label string for the passed in value.
         */
        QwtText label(double value) const;

        /**
         * @brief Stores the xaxis label history.
         * @remarks The array size has a limit of GraphPage::MAX_HISTORY
         */
        QList<QPair<double, QString> *> m_xLabels;

    }; // End TimeScaleDraw


    /// Contains the information for a variable plot.
    class PlotData
    {
    public:

        ///  Contructor for the plot data class.
        PlotData();

        ///  Destructor for the plot data class.
        ~PlotData();

        /// The qwt curve object that's displayed on the graph.
        QwtPlotCurve* curve;

        /// The color of the curve.
        QColor color;

        /// The DDS topic name.
        QString topicName;

        /// The DDS topic member name.
        QString variableName;

        /// The y-axis scaler value.
        double biasScale;

        /// The y-axis shift value.
        double biasShift;

        /// The x-axis history.
        double xData[MAX_HISTORY];

        /// The y-axis history.
        double yData[MAX_HISTORY];

        /// The x-axis data array that's displayed on the plot.
        double xViewData[MAX_HISTORY];

        /// The y-axis data array with the bias applied.
        double yViewData[MAX_HISTORY];

        /// Flag set to true when this data is used as the x-axis.
        bool isAxisData;
    };


    /**
     * @brief Return the plot with the passed in name.
     * @param[in] variableName The full VTS variable name of the variable.
     * @return A pointer to the plot with the given name or NULL if not found.
     */
    PlotData* getPlot(const QString& variableName);

    /// The graph properties dialog that controls the refresh rate and axis.
    QDialog* m_propertiesDialog;

    /// The UI object that provides access to the graph property widgets.
    Ui::GraphPropertiesPage* m_propertiesUI;

    /// The x-axis object that displays the time of each tick mark.
    TimeScaleDraw* m_xAxis;

    /// Manages the clicked point.
    QwtPicker* m_picker;

    /// Displays the clicked point for the selected variable.
    QwtPlotMarker* m_marker;

    /// Stores the data for the plots.
    QList<PlotData*> m_plotData;

    /// Redraw the graph when this timer expires.
    QTimer m_refreshTimer;

    /// Stores the x-axis data.
    double m_xData[MAX_HISTORY];

    /// Counts the number of times the plot has been drawn. Used for x-axis.
    double m_xCounter;

    /// Stores the lowest x value to display on the x-axis.
    double m_xMin;

    /// Stores the highest x value to display on the x-axis.
    double m_xMax;

}; // End GraphPage

#endif


/**
 * @}
 */
