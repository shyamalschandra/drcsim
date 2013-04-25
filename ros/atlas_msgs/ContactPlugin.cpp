/*
 * Copyright 2012 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/


#include "gazebo/transport/transport.hh"
#include "ContactPlugin.h"

using namespace gazebo;
GZ_REGISTER_MODEL_PLUGIN(ContactPlugin)

/////////////////////////////////////////////////
ContactPlugin::ContactPlugin() : ModelPlugin()
{
}

/////////////////////////////////////////////////
ContactPlugin::~ContactPlugin()
{
  this->contactSub.reset();
  this->contactsPub.reset();
  event::Events::DisconnectWorldUpdateBegin(this->updateConnection);
  this->collisions.clear();
}

/////////////////////////////////////////////////
void ContactPlugin::Load(physics::ModelPtr _model, sdf::ElementPtr _sdf)
{
  this->model = _model;
  this->world = _model->GetWorld();
  this->sdf = _sdf;

  std::string collisionName;
  std::string collisionScopedName;

  sdf::ElementPtr collisionElem =
    _sdf->GetElement("contact")->GetElement("collision");

  // Get all the collision elements
  while (collisionElem)
  {
    // get collision name
    collisionName = collisionElem->GetValueString();
    this->collisions.push_back(_model->GetName() + "::" + collisionName);
    collisionElem = collisionElem->GetNextElement("collision");
  }
}

//////////////////////////////////////////////////
void ContactPlugin::Init()
{

  this->node.reset(new transport::Node());
  this->node->Init(this->world->GetName());

  // Create a publisher for the contact information.
  if (this->sdf->HasElement("contact") &&
      this->sdf->GetElement("contact")->HasElement("topic") &&
      this->sdf->GetElement("contact")->GetValueString("topic")
      != "__default_topic__")
  {
    // This will create a topic based on the name specified in SDF.
    this->contactsPub = this->node->Advertise<msgs::Contacts>(
        this->sdf->GetElement("contact")->GetValueString("topic"));
  }
  else
  {
    // This will create a topic based on the name of the parent and the
    // name of the sensor.
    static int contactNum = 0;
    std::string topicName = "~/";
    topicName += this->model->GetName() + "/contact_" +
        boost::lexical_cast<std::string>(contactNum++);
    boost::replace_all(topicName, "::", "/");

    this->contactsPub = this->node->Advertise<msgs::Contacts>(topicName);
  }

  if (!this->contactSub)
  {
    this->contactSub = this->node->Subscribe("~/physics/contacts",
        &ContactPlugin::OnContacts, this);
  }

  this->updateConnection = event::Events::ConnectWorldUpdateBegin(
      boost::bind(&ContactPlugin::OnUpdate, this));
}


//////////////////////////////////////////////////
void ContactPlugin::OnUpdate()
{
  boost::mutex::scoped_lock lock(this->mutex);
  std::vector<std::string>::iterator collIter;
  std::string collision1;

  // Don't do anything if there is no new data to process.
  if (this->incomingContacts.size() == 0)
    return;

  // Clear the outgoing contact message.
  this->contactsMsg.clear_contact();

  // Iterate over all the contact messages
  for (ContactMsgs_L::iterator iter = this->incomingContacts.begin();
      iter != this->incomingContacts.end(); ++iter)
  {
    // Iterate over all the contacts in the message
    for (int i = 0; i < (*iter)->contact_size(); ++i)
    {
      collision1 = (*iter)->contact(i).collision1();

      // Try to find the first collision's name
      collIter = std::find(this->collisions.begin(),
          this->collisions.end(), collision1);

      // If unable to find the first collision's name, try the second
      if (collIter == this->collisions.end())
      {
        collision1 = (*iter)->contact(i).collision2();
        collIter = std::find(this->collisions.begin(),
            this->collisions.end(), collision1);
      }

      // If this model is monitoring one of the collision's in the
      // contact, then add the contact to our outgoing message.
      if (collIter != this->collisions.end())
      {
        int count = (*iter)->contact(i).position_size();

        // Check to see if the contact arrays all have the same size.
        if (count != (*iter)->contact(i).normal_size() ||
            count != (*iter)->contact(i).wrench_size() ||
            count != (*iter)->contact(i).depth_size())
        {
          gzerr << "Contact message has invalid array sizes\n";
          continue;
        }
        // Copy the contact message.
        msgs::Contact *contactMsg = this->contactsMsg.add_contact();
        contactMsg->CopyFrom((*iter)->contact(i));
      }
    }
  }

  // Clear the incoming contact list.
  this->incomingContacts.clear();

  // Generate a outgoing message only if someone is listening.
  if (this->contactsPub && this->contactsPub->HasConnections())
  {
//    msgs::Set(this->contactsMsg.mutable_time(), this->lastMeasurementTime);
    msgs::Set(this->contactsMsg.mutable_time(), this->world->GetSimTime());
    this->contactsPub->Publish(this->contactsMsg);
  }
}

//////////////////////////////////////////////////
void ContactPlugin::OnContacts(ConstContactsPtr &_msg)
{
  boost::mutex::scoped_lock lock(this->mutex);

  // Only store information if the sensor is active
  // if (this->IsActive())
  {
    // Store the contacts message for processing in UpdateImpl
    this->incomingContacts.push_back(_msg);

    // Prevent the incomingContacts list to grow indefinitely.
    if (this->incomingContacts.size() > 100)
      this->incomingContacts.pop_front();
  }
}
