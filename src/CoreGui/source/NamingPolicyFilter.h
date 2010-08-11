/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#ifndef NAMINGPOLICYFILTER_H
#define NAMINGPOLICYFILTER_H

#include "QtilitiesCoreGui_global.h"

#include <AbstractSubjectFilter.h>
#include <Factory.h>

#include <QItemDelegate>
#include <QValidator>

namespace Qtilities {
    namespace CoreGui {
        using namespace Qtilities::Core;
        class NamingPolicyInputDialog;

        /*!
        \class Qtilities::CoreGui::NamingPolicyFilter
        \brief The NamingPolicyFilter class is an implementation of AbstractSubjectFilter which allows control over naming of objects within the context of an Observer.

        The NamingPolicyFilter class is usefull when you need to control the names of subjects within the context of an Observer. Currently NamingPolicyFilter
        allows control over the validity and uniqueness of a names within the context of an Observer, set using setObserverContext().

        The validity of a name is determined by a QValdiator which is set using setValidator(). To get the current validator use getValidator() which returns a QValidator pointer
        which can be installed on QTextEdit controls directly. Valid names are those which return QValidator::Acceptable when validated using the validator. By default the
        NamingPolicySubject filter validates names using a QRegExpValidator constructed as follows:

\code
const QRegExp default_expression("\\.{1,100}",Qt::CaseInsensitive);
QRegExpValidator* default_validator = new QRegExpValidator(default_expression,0);
\endcode

        The uniqueness of names are determined by comparing the names of subjects using QString's compare (overloaded ==) functionality, thus comparisons are case sensitive. The uniqueness
        of names are controlled by the UniquenessPolicy of the subject filter. This can be accessed using setUniquenessPolicy() and uniquenessPolicy().

        It is important to note that the UniquenessPolicy and the subject's filter's validator can only be set while no subjects are attached within the subject filter's observer context.

        Once the NamingPolicyFilter is set up the way you need it, it will add a single shared property (see SharedObserverProperty) to subjects attached to it's observer context.
        The name of this property is defined in code using the Qtilities::Core::Properties::OBJECT_NAME constant.

        This dynamic property will be sync'ed with objectName() at all times. Since there is no way to know when setObjectName() is called on a QObject, a dynamic property
        needed to be added to manage the subject's name. Whenever you update this shared property it will be evaluated by the subject filter which will make
        sure the new name follows the validity of the filter's validator as well as the UniquenessPolicy of the filter.

        The diagram below shows a QObject which is observed by a number of observers with different, or no naming policy filters installed.

        \image html naming_policy_filter_overview.jpg "Naming Policy Filter Overview"
        \image latex naming_policy_filter_overview.eps "Naming Policy Filter Overview" width=\textwidth

        Subject names are evaulated during attachment to the filter's observer context and when changing the above dynamic property. Evaluation is done using the evaluateName() function
        which returns the validity of the name using a value defined by ValidityCheckResult. You can control how problematic names are handled by setting the
        the uniqueness resolution policy and the validity resolution policy of the subject filter. Access functions for these policies are setUniquenessResolutionPolicy(), uniquenessResolutionPolicy()
        setValidityResolutionPolicy() and validityResolutionPolicy(). Possible values for these policies are defined using ResolutionPolicy.

        Note: When streaming naming policy filter objects, custom validators will not be streamed at present. Thus the default validator will be used when constructing a naming policy filter from a binary stream.

        \sa NamingPolicyInputDialog
        \todo
        - Wishlist: Allowing control over unique name comparisons might be usefull in the future.
        - Wishlist: Implement a Replace resolution policy. There need to be decided how this should be done: Should the old subject be deleted, or just detached? If it is deleted do we add the new subject to all the observer's which observed the old subject? Also, the validity resolution policy cannot use replace so that must be handled correctly.
        */
        class QTILITIES_CORE_GUI_SHARED_EXPORT NamingPolicyFilter : public AbstractSubjectFilter
        {
            Q_OBJECT
            Q_ENUMS(UniquenessPolicy)
            Q_ENUMS(ResolutionPolicy)
            Q_ENUMS(ValidityCheckResult)

            friend class NamingPolicyInputDialog;

        public:
            NamingPolicyFilter(QObject* parent = 0);
            ~NamingPolicyFilter();
            QString filterName() { return tr("Naming Policy Filter"); }

            //! Policy to control uniqueness of subject names in the observer context in which this filter is installed.
            /*!
              \sa setUniquenessPolicy(), uniquenessNamingPolicy()
              */
            enum UniquenessPolicy {
                AllowDuplicateNames,    /*!< Allow duplicate names. */
                ProhibitDuplicateNames  /*!< Prohibit duplicate names. */
            };
            //! Policy to control how invalid or duplicate names should be handled.
            /*!
              setUniquenessPolicy(), uniquenessPolicy(), setUniquenessResolutionPolicy(), uniquenessResolutionPolicy(), setValidityResolutionPolicy(), validityResolutionPolicy()
              */
            enum ResolutionPolicy {
                AutoRename,             /*!< Automatically rename new names. \sa generateValidName() */
                PromptUser,             /*!< Bring up a Qtilities::CoreGui::NamingPolicyInputDialog widget from which the user can decide what to do. */
                Reject                  /*!< Reject unacceptable names. */
            };
            //! Enumeration defining the possible outcames of name evaluation operations.
            /*!
              \sa evaluateName()
              */
            enum ValidityCheckResult {
                Acceptable = 0,         /*!< The name is acceptable. */
                Duplicate = 1,          /*!< A duplicate name exists. */
                Invalid = 2             /*!< The name is invalid in this context. \sa setValidator(), getValidator(). */
            };
            Q_DECLARE_FLAGS(NameValidity, ValidityCheckResult);
            Q_FLAGS(NameValidity);

            // --------------------------------
            // Factory Interface Implemenation
            // --------------------------------
            static FactoryItem<AbstractSubjectFilter, NamingPolicyFilter> factory;

            //! Sets the naming uniqueness policy of this subject filter.
            void setUniquenessPolicy(NamingPolicyFilter::UniquenessPolicy naming_uniqueness_policy);
            //! Gets the naming uniqueness policy used by this subject filter.
            NamingPolicyFilter::UniquenessPolicy uniquenessNamingPolicy() const { return d_uniqueness_policy; }

            //! Sets the naming uniqueness conflict policy used by this subject filter.
            void setUniquenessResolutionPolicy(NamingPolicyFilter::ResolutionPolicy naming_uniqueness_resolution_policy);
            //! Gets the naming uniqueness conflict policy used by this subject filter.
            NamingPolicyFilter::ResolutionPolicy niquenessResolutionPolicy() const { return d_uniqueness_resolution_policy; }

            //! Sets the naming validity conflict policy used by this subject filter.
            void setValidityResolutionPolicy(NamingPolicyFilter::ResolutionPolicy naming_validity_resolution_policy);
            //! Gets the naming validity conflict policy used by this subject filter.
            NamingPolicyFilter::ResolutionPolicy validityResolutionPolicy() const { return d_validity_resolution_policy; }

            AbstractSubjectFilter::EvaluationResult evaluateAttachment(QObject* obj) const;
            bool initializeAttachment(QObject* obj);
            void finalizeAttachment(QObject* obj, bool attachment_successful);
            AbstractSubjectFilter::EvaluationResult evaluateDetachment(QObject* obj) const;
            bool initializeDetachment(QObject* obj, bool subject_deleted = false) { return true; }
            void finalizeDetachment(QObject* obj, bool detachment_successful, bool subject_deleted = false);

            QStringList monitoredProperties();
            bool monitoredPropertyChanged(QObject* obj, const char* property_name, QDynamicPropertyChangeEvent* propertyChangeEvent);

            bool exportFilterSpecificBinary(QDataStream& stream) const;
            bool importFilterSpecificBinary(QDataStream& stream);

            //! Evaluates a name in the observer context in which this subject filter is installed.
            NamingPolicyFilter::NameValidity evaluateName(QString name) const;
            //! Gets the object which conflicts with the specified name. If no object conflicts, returns 0.
            QObject* getConflictingObject(QString name) const;
            //! Function to set the validator used to validate names.
            void setValidator(QValidator* valid_naming_validator);
            //! Gets the validator used to validate names.
            QValidator* const getValidator();

            //! Function which makes this naming policy filter the object name manager of the given object.
            void makeNameManager(QObject* obj);

            //! Function which starts a new naming validation cycle.
            /*!
              A naming validation cycle reffers to a bunch of validations that must happen. For example if you
              are parsing an input file which results in the attachment of many subjects to the observer context
              which this filter is validating, you create a validation cycle for this process.

              Validation cycles allow the use of the "All in this cycle" option in the NamingPolicyInputDialog.
              When checked, you may choose to use one option for all subjects in this cycle and the user does not
              need to select an option for each file.

              An cycle is ended by calling endValidationCycle(). If you call startValidationCycle() while
              a cycle is already active the function has no effect. Thus you don't have to match the number of
              startValidationCycle() calls with the equal number of endValidationCycle() calls.
              */
            void startValidationCycle();
            //! Function which ends a naming validation cycle.
            /*!
              Ends a validation cycle started with startValidationCycle(). If no cycle was started this function does
              nothing.
              */
            void endValidationCycle();
            //! Returns true if a validation cycle is active at present.
            bool isValidationCycleActive() const;

        private:
            //! Attempt to assign a new name manager to the object, other than this filter.
            void assignNewNameManager(QObject* obj);
            //! Checks if this subject filter is the name manager of the specified object.
            bool isObjectNameManager(QObject* obj) const;
            //! Check if the property actually changed during monitoredPropertyChanged() function call, thus check objectName() against the OBJECT_NAME property.
            bool isObjectNameDirty(QObject* obj) const;
            //! Attempt to generate a valid name in the context from the given input_name.
            /*!
              The valid name generation attempts the following in the order shown.

              When invalid:
              - Attempt to fix the string using the QRegExpValidator::fixup() function
              - Go on to the steps shown below.

              When invalid and unique: (fixup worked)
              - Append the value of a counter to the name

              When valid and duplicate: (fixup failed)
              - Attempt to remove whitespaces
              - Try a simple string: new_object
              - Try a simple string without an underscore: NewObject

              This function takes the UniquenessPolicy of the subject filter into account when generating a valid name.

              \return A valid QString value. If QString is returned empty the function could not succeed in generating a valid name.
              */
            QString generateValidName(QString input_name = QString(), bool force_change = false);
            bool validateNamePropertyChange(QObject* obj, const char* property_name);
            QValidator* validator;
            NamingPolicyInputDialog* name_dialog;

            QString rollback_name;
            bool validation_cycle_active;
            NamingPolicyFilter::UniquenessPolicy d_uniqueness_policy;
            NamingPolicyFilter::ResolutionPolicy d_uniqueness_resolution_policy;
            NamingPolicyFilter::ResolutionPolicy d_validity_resolution_policy;
        };

        Q_DECLARE_OPERATORS_FOR_FLAGS(NamingPolicyFilter::NameValidity)
									
        /*!
        \class Qtilities::CoreGui::NamingPolicyDelegateData
        \brief The NamingPolicyDelegateData class provides stores data needed by the NamingPolicyDelegate class.
          */
        class NamingPolicyDelegateData;

        /*!
        \class Qtilities::CoreGui::NamingPolicyDelegate
        \brief The NamingPolicyDelegate class provides a ready-to-use delegate to edit the names of subjects attached to an observer.

        The naming policy delegate is used to allow editing of subject names in a text editor which is sensitive
        to a context's naming policy filter. Below is an example of the delegate in action in an observer widget.

        \image html observer_widget_naming_delegate.jpg "Observer Widget Handling Context Naming Policies"
        \image latex observer_widget_naming_delegate.eps "Observer Widget Handling Context Naming Policies" width=4in
        */
         class QTILITIES_CORE_GUI_SHARED_EXPORT NamingPolicyDelegate : public QItemDelegate
         {
             Q_OBJECT

         public:
             NamingPolicyDelegate(QObject *parent = 0);

            //! Sets the observer context for this delegate.
            void setObserverContext(Observer* observer);
            //! Gets a pointer to the observer context used by this delegate.
            Observer* observerContext() const;

            QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
            void setEditorData(QWidget *editor, const QModelIndex &index) const;
            void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
            void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;

         public slots:
            void on_LineEdit_TextChanged(const QString & text);
            void handleCurrentObjectChanged(QList<QObject*> obj);

         private:
            NamingPolicyDelegateData* d;
         };									
    }
}

#endif // NAMINGPOLICYFILTER_H