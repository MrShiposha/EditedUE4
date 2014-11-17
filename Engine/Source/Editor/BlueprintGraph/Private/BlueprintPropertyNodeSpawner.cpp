// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "BlueprintPropertyNodeSpawner.h"
#include "K2Node_Variable.h"
#include "EditorStyleSettings.h"	// for bShowFriendlyNames
#include "ObjectEditorUtils.h"		// for GetCategory()
#include "BlueprintEditorUtils.h"	// for FindBlueprintForNodeChecked()

#define LOCTEXT_NAMESPACE "BlueprintPropertyNodeSpawner"

/*******************************************************************************
 * UBlueprintPropertyNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintPropertyNodeSpawner* UBlueprintPropertyNodeSpawner::Create(UProperty const* const Property, UObject* Outer/* = nullptr*/)
{
	check(Property != nullptr);

	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	UBlueprintPropertyNodeSpawner* NodeSpawner = NewObject<UBlueprintPropertyNodeSpawner>(Outer);
	NodeSpawner->Property  = Property;
	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintPropertyNodeSpawner::UBlueprintPropertyNodeSpawner(class FPostConstructInitializeProperties const& PCIP)
	: Super(PCIP)
	, Property(nullptr)
{
}

//------------------------------------------------------------------------------
UEdGraphNode* UBlueprintPropertyNodeSpawner::Invoke(UEdGraph* ParentGraph) const
{
	// if a NodeClass was set, then this should spawn something
	UEdGraphNode* NewNode = Super::Invoke(ParentGraph);
	check(Property != nullptr);

	if (NewNode != nullptr)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(NewNode);
		UClass* OwnerClass = Property->GetOwnerClass();
		bool const bIsSelfContext = Blueprint->SkeletonGeneratedClass->IsChildOf(OwnerClass);

		if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(NewNode))
		{
			VarNode->SetFromProperty(Property, bIsSelfContext);
		}
		else if (UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(NewNode))
		{
			DelegateNode->SetFromProperty(Property, bIsSelfContext);
		}
	}
	// else, without a NodeClass explicitly specified, we don't know what kind  
	// of property node to spawn (for example: a getter versus a setter)... 
	// we can use a null NodeClass to represent the property itself (an 
	// accumulation of all the nodes that it COULD spawn), and the ui can then 
	// provide the user with a sub-menu to pick from (but it is up to some 
	// external system to interpret/use it that way)

	return NewNode;
}

//------------------------------------------------------------------------------
FText UBlueprintPropertyNodeSpawner::GetDefaultMenuName() const
{
	check(Property != nullptr);
	
	FText MenuName;
	if (GetDefault<UEditorStyleSettings>()->bShowFriendlyNames)
	{
		MenuName = FText::FromString(UEditorEngine::GetFriendlyName(Property));
	}
	else
	{
		MenuName = FText::FromName(Property->GetFName());
	}

	if (NodeClass != nullptr)
	{
		if (NodeClass->IsChildOf<UK2Node_VariableGet>())
		{
			MenuName = FText::Format(LOCTEXT("GetterMenuName", "Get {0}"), MenuName);
		}
		else if (NodeClass->IsChildOf<UK2Node_VariableSet>())
		{
			MenuName = FText::Format(LOCTEXT("SetterMenuName", "Set {0}"), MenuName);
		}
	}

	return MenuName;
}

//------------------------------------------------------------------------------
FText UBlueprintPropertyNodeSpawner::GetDefaultMenuCategory() const
{
	check(Property != nullptr);

	FText PropertyCategory = FText::FromString(FObjectEditorUtils::GetCategory(Property));
	if (!IsDelegateProperty())
	{
		PropertyCategory = FText::Format(LOCTEXT("VariableCategory", "Variables|{0}"), PropertyCategory);
	}
	else if (PropertyCategory.IsEmpty())
	{
		PropertyCategory = LOCTEXT("DefaultDelegateCategory", "Event Dispatchers");
	}
	return PropertyCategory;
}

//------------------------------------------------------------------------------
FName UBlueprintPropertyNodeSpawner::GetDefaultMenuIcon(FLinearColor& ColorOut)
{
	FName BrushName = Super::GetDefaultMenuIcon(ColorOut);
	if (NodeClass == nullptr)
	{
		if (IsDelegateProperty())
		{
			BrushName = TEXT("GraphEditor.Delegate_16x");
		}
	}

	return BrushName;
}

//------------------------------------------------------------------------------
UProperty const* UBlueprintPropertyNodeSpawner::GetProperty() const
{
	return Property;
}

//------------------------------------------------------------------------------
bool UBlueprintPropertyNodeSpawner::IsDelegateProperty() const
{
	check(Property != nullptr);
	return Property->IsA(UMulticastDelegateProperty::StaticClass());
}

#undef LOCTEXT_NAMESPACE